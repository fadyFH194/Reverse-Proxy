// route_manager.h

#ifndef ROUTE_MANAGER_H
#define ROUTE_MANAGER_H

#include <stddef.h>  // Defines size_t

typedef struct {
    char prefix[1024];
    char host[128];
    int port;
} Route;

// Adds a new route to the routing table
int add_route(const char *prefix, const char *host, int port);

// Retrieves the target host, port, and matched prefix based on the request path
int get_route(const char *path, char *host, int *port, char *matched_prefix, size_t prefix_size);

// Forwards the modified HTTP request to the target server
int forward_request(const char *host, int port, const char *request);

#endif // ROUTE_MANAGER_H
