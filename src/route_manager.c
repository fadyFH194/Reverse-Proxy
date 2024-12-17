// route_manager.c

#include "route_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <errno.h>

#define MAX_ROUTES 100
#define READ_TIMEOUT 10    // Adjusted for better performance
#define WRITE_TIMEOUT 10   // Adjusted for better performance

static Route routes[MAX_ROUTES];
static int route_count = 0;

// Add a new route to the routing table
int add_route(const char *prefix, const char *host, int port) {
    if (route_count >= MAX_ROUTES) {
        fprintf(stderr, "Route table is full.\n");
        return -1;
    }

    strncpy(routes[route_count].prefix, prefix, sizeof(routes[route_count].prefix) - 1);
    routes[route_count].prefix[sizeof(routes[route_count].prefix) - 1] = '\0';

    strncpy(routes[route_count].host, host, sizeof(routes[route_count].host) - 1);
    routes[route_count].host[sizeof(routes[route_count].host) - 1] = '\0';

    routes[route_count].port = port;
    route_count++;

    // Debug output
    printf("Route added: prefix=%s, host=%s, port=%d\n", prefix, host, port);
    return 0;
}

// Retrieve the target host, port, and matched prefix based on the request path
int get_route(const char *path, char *host, int *port, char *matched_prefix, size_t prefix_size) {
    for (int i = 0; i < route_count; i++) {
        size_t prefix_len = strlen(routes[i].prefix);
        if (strncmp(path, routes[i].prefix, prefix_len) == 0) {
            strncpy(host, routes[i].host, 128);
            host[127] = '\0';
            *port = routes[i].port;
            strncpy(matched_prefix, routes[i].prefix, prefix_size - 1);
            matched_prefix[prefix_size - 1] = '\0';
            printf("Route found for path %s: %s:%d with prefix %s\n", path, host, *port, matched_prefix);
            return 0;
        }
    }
    printf("No route found for path %s\n", path);
    return -1; // Route not found
}

// Forward the modified HTTP request to the target server
int forward_request(const char *host, int port, const char *request) {
    int sockfd;
    struct sockaddr_in server_addr;
    struct hostent *server;

    // Resolve host using gethostbyname
    server = gethostbyname(host);
    if (server == NULL) {
        fprintf(stderr, "No such host: %s\n", host);
        return -1;
    }

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Set socket timeouts to prevent hanging
    struct timeval timeout;
    timeout.tv_sec = READ_TIMEOUT;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt SO_RCVTIMEO failed");
        // Proceed without timeout
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt SO_SNDTIMEO failed");
        // Proceed without timeout
    }

    // Prepare server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    // Connect to target server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect to target server failed");
        close(sockfd);
        return -1;
    }

    // Send request to target server
    size_t total_sent = 0;
    size_t request_len = strlen(request);
    while (total_sent < request_len) {
        ssize_t sent = send(sockfd, request + total_sent, request_len - total_sent, 0);
        if (sent < 0) {
            perror("Failed to send to target server");
            close(sockfd);
            return -1;
        }
        total_sent += sent;
    }

    printf("Request forwarded to target server at %s:%d\n", host, port);
    return sockfd; // Return the socket to read the response
}
