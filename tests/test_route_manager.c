#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>
#include "route_manager.h"

#define MAX_ROUTES 100 // Define MAX_ROUTES

extern void reset_routes_for_test(); 

static int test_add_and_get_route();
static int test_route_table_full();
static int test_forward_request_errors();

int main() {
    int result = 0;

    result |= test_add_and_get_route();
    result |= test_route_table_full();
    result |= test_forward_request_errors();

    if (result == 0) {
        printf("All route_manager tests passed.\n");
    } else {
        printf("Some route_manager tests failed.\n");
    }

    return result;
}

static int test_add_and_get_route() {
    // Start fresh
    reset_routes_for_test();

    int result = add_route("/test", "127.0.0.1", 4000);
    if (result != 0) {
        fprintf(stderr, "Failed to add route\n");
        return 1;
    }

    char host[128];
    int port;
    if (get_route("/test", host, &port) != 0) {
        fprintf(stderr, "Failed to get route that should exist\n");
        return 1;
    }

    if (strcmp(host, "127.0.0.1") != 0 || port != 4000) {
        fprintf(stderr, "Got incorrect route data\n");
        return 1;
    }

    // Test a route that does not exist
    if (get_route("/doesnotexist", host, &port) == 0) {
        fprintf(stderr, "Should not find a non-existing route\n");
        return 1;
    }

    return 0;
}

static int test_route_table_full() {
    // Reset before filling
    reset_routes_for_test();

    // Fill up the route table
    for (int i = 0; i < MAX_ROUTES; i++) {
        char prefix[32];
        snprintf(prefix, sizeof(prefix), "/route%d", i);
        if (add_route(prefix, "127.0.0.1", 4000) != 0) {
            fprintf(stderr, "Failed to add route at index %d\n", i);
            return 1;
        }
    }

    // Now table is full, next add_route should fail
    if (add_route("/overflow", "127.0.0.1", 4000) == 0) {
        fprintf(stderr, "Expected route table to be full\n");
        return 1;
    }

    return 0;
}

static int test_forward_request_errors() {
    // Test invalid host (inet_pton failure)
    int socket_fd = forward_request("invalid_host", 80, "GET / HTTP/1.1\r\n\r\n");
    if (socket_fd != -1) {
        fprintf(stderr, "Expected forward_request to fail with invalid host\n");
        return 1;
    }

    // Test invalid port (e.g., negative port)
    socket_fd = forward_request("127.0.0.1", -1, "GET / HTTP/1.1\r\n\r\n");
    if (socket_fd != -1) {
        fprintf(stderr, "Expected forward_request to fail with invalid port\n");
        return 1;
    }

    // Test connection failure (assuming no server is running on port 9999)
    socket_fd = forward_request("127.0.0.1", 9999, "GET / HTTP/1.1\r\n\r\n");
    if (socket_fd != -1) {
        fprintf(stderr, "Expected forward_request to fail to connect to port 9999\n");
        return 1;
    }

    // Since we cannot reliably test write failures without mocking,
    // we can consider this sufficient for coverage purposes.

    return 0;
}