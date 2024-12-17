// proxy.c

#include "proxy.h"
#include "route_manager.h"
#include "json_parser.h" // For JSON parsing
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>    // For sockaddr_in, htons, etc.
#include <pthread.h>      // For pthread_t
#include <ctype.h>
#include <semaphore.h>    // For sem_t, sem_open, sem_wait, sem_post, sem_close, sem_unlink
#include <fcntl.h>        // For O_CREAT, O_EXCL
#include <sys/stat.h>     // For mode constants
#include <signal.h>       // For signal handling
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#define BUFFER_SIZE 8192
#define MAX_CONCURRENT_CONNECTIONS 100
#define SEM_NAME "/reverse_proxy_sem"
#define READ_TIMEOUT 10    // Increased from 5 seconds
#define WRITE_TIMEOUT 10   // Increased from 5 seconds

static sem_t *connection_sem = NULL;
volatile sig_atomic_t stop_server = 0;

// Function Prototypes
void handle_signal(int signum);
static int parse_headers(const char *request, char *method, size_t method_size,
                         char *path, size_t path_size, char *version, size_t version_size,
                         char **headers_start);
static int get_content_length(const char *headers);
static const char *find_body(const char *headers);
void *handle_client_request(void *arg);

// Signal Handler to gracefully stop the server
void handle_signal(int signum) {
    (void)signum; // Unused parameter
    stop_server = 1;
}

// Start the Proxy Server
int start_proxy(int port, void *(*handler)(void *)) {
    int server_socket;
    struct sockaddr_in server_addr;

    // Set up signal handlers for graceful shutdown
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sa.sa_flags = 0; 
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Create named semaphore to limit concurrent connections
    sem_unlink(SEM_NAME);
    connection_sem = sem_open(SEM_NAME, O_CREAT, 0644, MAX_CONCURRENT_CONNECTIONS);
    if (connection_sem == SEM_FAILED) {
        perror("sem_open failed");
        return -1;
    }

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        sem_close(connection_sem);
        sem_unlink(SEM_NAME);
        return -1;
    }

    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        close(server_socket);
        sem_close(connection_sem);
        sem_unlink(SEM_NAME);
        return -1;
    }

    // Bind socket to the specified port
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Socket bind failed");
        close(server_socket);
        sem_close(connection_sem);
        sem_unlink(SEM_NAME);
        return -1;
    }

    // Start listening on the socket
    if (listen(server_socket, 1024) < 0) {
        perror("Socket listen failed");
        close(server_socket);
        sem_close(connection_sem);
        sem_unlink(SEM_NAME);
        return -1;
    }

    printf("Proxy server running on port %d...\n", port);

    // Main accept loop
    while (!stop_server) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        // Wait if maximum concurrent connections are reached
        if (sem_wait(connection_sem) < 0) {
            if (stop_server) break;
            perror("sem_wait failed");
            continue;
        }

        // Accept incoming client connection
        int *client_socket = malloc(sizeof(int));
        if (!client_socket) {
            perror("Failed to allocate memory for client_socket");
            sem_post(connection_sem);
            continue;
        }

        *client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
        if (*client_socket < 0) {
            perror("Socket accept failed");
            free(client_socket);
            sem_post(connection_sem);
            continue;
        }

        // Create a detached thread to handle the client request
        pthread_t thread;
        if (pthread_create(&thread, NULL, handler, client_socket) != 0) {
            perror("pthread_create failed");
            close(*client_socket);
            free(client_socket);
            sem_post(connection_sem);
            continue;
        }
        pthread_detach(thread);
    }

    // Clean up resources
    close(server_socket);
    sem_close(connection_sem);
    sem_unlink(SEM_NAME);
    printf("Proxy server stopped.\n");

    return 0;
}

// Handle Client Requests with Persistent Connections
void *handle_client_request(void *arg) {
    int client_socket = *(int *)arg;
    free(arg); 

    // Set socket read and write timeouts to prevent hanging
    struct timeval timeout;
    timeout.tv_sec = READ_TIMEOUT;
    timeout.tv_usec = 0;
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt SO_RCVTIMEO failed");
        // Proceed without timeout
    }
    if (setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt SO_SNDTIMEO failed");
        // Proceed without timeout
    }

    char buffer[BUFFER_SIZE];
    int keep_alive = 1; // Default to keep-alive for HTTP/1.1

    while (keep_alive) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);

        if (bytes_read < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // Timeout reached, close connection
                printf("Read timeout reached. Closing connection.\n");
                break;
            }
            perror("Failed to read from client");
            break;
        } else if (bytes_read == 0) {
            // Client closed connection
            printf("Client closed the connection.\n");
            break;
        }

        buffer[bytes_read] = '\0';

        char method[16], path[1024], version[16];
        char *headers_start = NULL;

        if (parse_headers(buffer, method, sizeof(method), path, sizeof(path),
                         version, sizeof(version), &headers_start) < 0) {
            const char *error_response = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
            write(client_socket, error_response, strlen(error_response));
            printf("Bad request received. Closing connection.\n");
            break; // Close connection after bad request
        }

        // Determine if the connection should be kept alive
        if (strcasecmp(version, "HTTP/1.1") == 0) {
            keep_alive = 1; // Default in HTTP/1.1
            if (strcasestr(headers_start, "Connection: close")) {
                keep_alive = 0;
            }
        } else if (strcasecmp(version, "HTTP/1.0") == 0) {
            keep_alive = 0; // Default in HTTP/1.0
            if (strcasestr(headers_start, "Connection: keep-alive")) {
                keep_alive = 1;
            }
        }

        printf("Received request: %s %s %s\n", method, path, version);

        // Handle the /register route
        if (strcasecmp(method, "POST") == 0 && strcmp(path, "/register") == 0) {
            int content_length = get_content_length(headers_start ? headers_start : "");
            if (content_length <= 0) {
                const char *error_response = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nMissing or invalid Content-Length.\r\n";
                write(client_socket, error_response, strlen(error_response));
                printf("Missing or invalid Content-Length. Closing connection.\n");
                break; // Close connection after bad request
            }

            const char *body = find_body(buffer);
            if (!body) {
                const char *error_response = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nNo body found.\r\n";
                write(client_socket, error_response, strlen(error_response));
                printf("No body found in /register request. Closing connection.\n");
                break; // Close connection after bad request
            }

            char prefix[1024], host[128];
            int port = 0;
            if (parse_json_for_route(body, prefix, sizeof(prefix), host, sizeof(host), &port) < 0) {
                const char *error_response = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nInvalid JSON.\r\n";
                write(client_socket, error_response, strlen(error_response));
                printf("Invalid JSON in /register request. Closing connection.\n");
                break; // Close connection after bad request
            }

            if (add_route(prefix, host, port) < 0) {
                const char *error_response = "HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\nFailed to add route.\r\n";
                write(client_socket, error_response, strlen(error_response));
                printf("Failed to add route. Closing connection.\n");
                break; // Close connection after server error
            }

            const char *success_response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: keep-alive\r\n\r\nRegistered.\r\n";
            write(client_socket, success_response, strlen(success_response));
            printf("Route registered successfully.\n");
            continue; // Continue to handle next request if keep_alive is true
        }

        // Handle normal route forwarding
        char target_host[128];
        int target_port;
        char matched_prefix[1024];
        if (get_route(path, target_host, &target_port, matched_prefix, sizeof(matched_prefix)) < 0) {
            const char *error_response = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
            write(client_socket, error_response, strlen(error_response));
            printf("No route found for path: %s. Closing connection.\n", path);
            break; // Close connection after not found
        }

        printf("Routing request to %s:%d with matched prefix: %s\n", target_host, target_port, matched_prefix);

        // Adjust the path based on the matched prefix
        char *new_path = path + strlen(matched_prefix);
        if (*new_path == '\0') {
            new_path = "/";
        }

        // Ensure that the new path starts with '/'
        if (*new_path != '/') {
            char temp_path[1024];
            snprintf(temp_path, sizeof(temp_path), "/%s", new_path);
            new_path = temp_path;
        }

        // Rewrite the request with the adjusted path and updated Host header
        char *host_header_start = strcasestr(headers_start, "Host:");
        char modified_request[BUFFER_SIZE * 2]; // Make bigger in case we add headers

        if (!host_header_start) {
            // If there's no Host header, add one along with Connection: close
            snprintf(modified_request, sizeof(modified_request),
                     "%s %s %s\r\nHost: %s:%d\r\nConnection: close\r\n%s",
                     method, new_path, version, target_host, target_port, headers_start);
        } else {
            // Replace the existing Host header and add Connection: close
            char *end_of_line = strstr(host_header_start, "\r\n");
            if (!end_of_line) end_of_line = host_header_start + strlen(host_header_start);

            // Copy request line
            char request_line[2048];
            snprintf(request_line, sizeof(request_line), "%s %s %s\r\n", method, new_path, version);

            // Headers after the old Host line
            char *after_host = end_of_line;
            if (after_host[0] == '\r' && after_host[1] == '\n') after_host += 2; // skip CRLF

            // Rebuild the request with the new Host and Connection headers
            snprintf(modified_request, sizeof(modified_request),
                     "%sHost: %s:%d\r\nConnection: close\r\n%s",
                     request_line, target_host, target_port, after_host);
        }

        // Log the modified request being sent to the target server
        printf("Modified request to target server:\n%s", modified_request);

        // Forward the modified request to the target server
        int target_socket = forward_request(target_host, target_port, modified_request);
        if (target_socket < 0) {
            const char *error_response = "HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n";
            write(client_socket, error_response, strlen(error_response));
            printf("Failed to forward request to target server. Closing connection.\n");
            break; // Close connection after bad gateway
        }

        printf("Request forwarded to target server.\n");

        // Relay the response back to the client
        while (1) {
            int bytes = read(target_socket, buffer, BUFFER_SIZE);
            if (bytes < 0) {
                perror("Failed to read from target server");
                printf("Error number: %d (%s)\n", errno, strerror(errno));
                break;
            } else if (bytes == 0) {
                // Target server closed connection
                printf("Target server closed the connection.\n");
                break;
            }

            int total_sent = 0;
            while (total_sent < bytes) {
                int sent = write(client_socket, buffer + total_sent, bytes - total_sent);
                if (sent < 0) {
                    perror("Failed to write to client");
                    printf("Error number: %d (%s)\n", errno, strerror(errno));
                    break;
                }
                total_sent += sent;
            }

            if (total_sent < bytes) {
                // Failed to send all data
                printf("Failed to send all data to client.\n");
                break;
            }
        }

        close(target_socket);

        if (!keep_alive) {
            printf("Closing connection as per keep_alive flag.\n");
            break; // Exit the loop to close the connection
        }
    }

    // Close client socket and release semaphore
    close(client_socket);
    sem_post(connection_sem);
    printf("Connection closed.\n");
    return NULL;
}

// Parse HTTP Headers
static int parse_headers(const char *request, char *method, size_t method_size,
                         char *path, size_t path_size, char *version, size_t version_size,
                         char **headers_start)
{
    (void)method_size; (void)path_size; (void)version_size;
    const char *end_of_line = strstr(request, "\r\n");
    if (!end_of_line) return -1;

    size_t line_len = end_of_line - request;
    if (line_len >= BUFFER_SIZE) return -1; // Prevent buffer overflow

    char first_line[2048];
    if (line_len >= sizeof(first_line)) return -1;
    memcpy(first_line, request, line_len);
    first_line[line_len] = '\0';

    if (sscanf(first_line, "%15s %1023s %15s", method, path, version) != 3) {
        return -1;
    }

    const char *headers = end_of_line + 2; // skip \r\n
    *headers_start = (char*)headers;
    return 0;
}

// Get Content-Length from Headers
static int get_content_length(const char *headers) {
    const char *cl = strcasestr(headers, "Content-Length:");
    if (!cl) return -1;
    cl += strlen("Content-Length:");
    while (*cl && isspace((unsigned char)*cl)) cl++;
    int length = atoi(cl);
    return length;
}

// Find Body in HTTP Request
static const char *find_body(const char *headers) {
    const char *sep = strstr(headers, "\r\n\r\n");
    if (!sep) return NULL;
    return sep + 4;
}
