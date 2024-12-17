// main.c

#include "proxy.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int port = 8080; // Default port

    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number.\n");
            return EXIT_FAILURE;
        }
    }

    printf("Starting reverse proxy on port %d...\n", port);

    if (start_proxy(port, handle_client_request) < 0) {
        fprintf(stderr, "Failed to start proxy server.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
