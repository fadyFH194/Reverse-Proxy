#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>

#include "proxy.h"
#include "route_manager.h"
#include "json_parser.h"

#define PORT 8080
#define TEST_HOST "127.0.0.1"

static pthread_t proxy_thread;
static pthread_t backend_thread;
static volatile int proxy_running = 0;
static volatile int backend_running = 0;

// A simple backend server that listens on 127.0.0.1:4000 and returns a fixed response.
// This ensures forward_request and response relaying code is covered.
static void *backend_server_func(void *arg) {
    (void)arg;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return NULL;
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4000);
    addr.sin_addr.s_addr = inet_addr(TEST_HOST);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        return NULL;
    }
    listen(server_fd, 10);
    backend_running = 1;
    while (backend_running) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd >= 0) {
            const char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, world!";
            write(client_fd, resp, strlen(resp));
            close(client_fd);
        }
    }
    close(server_fd);
    return NULL;
}

// The thread function that starts the proxy server
static void *proxy_thread_func(void *arg) {
    (void)arg;
    start_proxy(PORT, handle_client_request);
    proxy_running = 0;
    return NULL;
}

// Helper function to start the proxy server in another thread
static int start_proxy_server() {
    proxy_running = 1;
    if (pthread_create(&proxy_thread, NULL, proxy_thread_func, NULL) != 0) {
        perror("Failed to create proxy thread");
        return -1;
    }
    usleep(500000); // Wait for the server to start
    return 0;
}

// Helper function to stop the proxy server gracefully
static void stop_proxy_server() {
    if (proxy_running) {
        kill(getpid(), SIGINT);
        pthread_join(proxy_thread, NULL);
    }
}

// Helper function to send a request to the proxy and print the response via TCP
static void send_request_tcp(const char *request, const char *desc) {
    printf("---- %s (TCP) ----\n", desc);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, TEST_HOST, &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect failed");
        close(sock);
        return;
    }

    if (write(sock, request, strlen(request)) < 0) {
        perror("write to socket failed");
        close(sock);
        return;
    }

    char buf[2048];
    ssize_t n = read(sock, buf, sizeof(buf)-1);
    if (n > 0) {
        buf[n] = '\0';
        printf("Response:\n%s\n", buf);
    } else {
        printf("No response or error reading response.\n");
    }

    close(sock);
}

// Directly test handle_client_request using a socketpair to ensure coverage
// of various code paths even without running the full proxy loop.
static void test_handle_client_direct(const char *request, const char *desc) {
    printf("---- %s (Direct) ----\n", desc);
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair failed");
        return;
    }

    // Write request to sv[0]
    if (write(sv[0], request, strlen(request)) < 0) {
        perror("write to socket failed");
        close(sv[0]);
        close(sv[1]);
        return;
    }

    // Prepare argument
    int *client_socket = malloc(sizeof(int));
    *client_socket = sv[1];

    // Call handle_client_request directly
    handle_client_request(client_socket);

    // Read the response from sv[0]
    char buf[2048];
    ssize_t n = read(sv[0], buf, sizeof(buf)-1);
    if (n > 0) {
        buf[n] = '\0';
        printf("Response:\n%s\n", buf);
    } else {
        printf("No response or error reading response.\n");
    }

    close(sv[0]);
    // sv[1] closed in handle_client_request
}

int main() {
    // Start a backend server to ensure forward_request and response relay gets covered
    pthread_create(&backend_thread, NULL, backend_server_func, NULL);
    usleep(200000); // Wait for backend to start

    // Test direct handle_client_request calls first (no proxy loop)
    test_handle_client_direct("GET /nohost HTTP/1.1\r\n\r\n", "Direct: No Host header");
    test_handle_client_direct("BADREQUEST\r\n\r\n", "Direct: Malformed request line");
    test_handle_client_direct("POST /register HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n",
                              "Direct: POST /register with zero Content-Length");
    test_handle_client_direct("POST /register HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n{}",
                              "Direct: Valid POST /register with empty JSON");
    test_handle_client_direct("GET /notfound HTTP/1.1\r\nHost: localhost\r\n\r\n", 
                              "Direct: Route not found");
    test_handle_client_direct("GET /4000 HTTP/1.1\r\nHost: localhost\r\n\r\n",
                              "Direct: Known route forwarding (should 502 if backend not running)");
    test_handle_client_direct("POST /register HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: 50\r\n\r\n{\"prefix\":\"/invalid\",\"host\":\"localhost\",\"port\":-1}",
                              "Direct: Invalid port in JSON");

    // Now start the full proxy server
    if (start_proxy_server() < 0) {
        fprintf(stderr, "Failed to start proxy server.\n");
        backend_running = 0;
        pthread_join(backend_thread, NULL);
        return 1;
    }

    // Register routes so the proxy knows where to forward
    add_route("/4000", "127.0.0.1", 4000);

    // Test a normal route forwarding scenario over TCP
    // This should hit forward_request and since backend is running, return "Hello, world!"
    send_request_tcp("GET /4000 HTTP/1.1\r\nHost: localhost\r\n\r\n",
                     "TCP: Normal route forwarding with backend response");

    // Test POST /register scenarios over TCP
    send_request_tcp("POST /register HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\n\r\n{}",
                     "TCP: Missing Content-Length on POST /register");
    send_request_tcp("POST /register HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: 20\r\n\r\n{\"invalid_json\": }",
                     "TCP: Invalid JSON on POST /register");

    // Trigger signal to stop the proxy server after a request
    send_request_tcp("GET /4000 HTTP/1.1\r\nHost: localhost\r\n\r\n",
                     "TCP: Triggering another request before stop");

    // Stop the proxy server (signal handling)
    stop_proxy_server();

    // Stop the backend
    backend_running = 0;
    pthread_join(backend_thread, NULL);

    printf("All test scenarios executed.\n");
    return 0;
}
