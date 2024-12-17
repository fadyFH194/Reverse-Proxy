#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "json_parser.h"

void test_parse_json_for_route() {
    char json[] = "{\"prefix\": \"/test\", \"host\": \"127.0.0.1\", \"port\": 8080}";
    char prefix[1024];
    char host[128];
    int port;

    int result = parse_json_for_route(json, prefix, sizeof(prefix), host, sizeof(host), &port);
    assert(result == 0);
    assert(strcmp(prefix, "/test") == 0);
    assert(strcmp(host, "127.0.0.1") == 0);
    assert(port == 8080);

    printf("test_parse_json_for_route passed.\n");
}

void test_parse_invalid_json() {
    char json[] = "{\"prefix\": \"/test\", \"host\": 127.0.0.1, \"port\": }";
    char prefix[1024];
    char host[128];
    int port;

    int result = parse_json_for_route(json, prefix, sizeof(prefix), host, sizeof(host), &port);
    assert(result != 0);

    printf("test_parse_invalid_json passed.\n");
}

int main() {
    test_parse_json_for_route();
    test_parse_invalid_json();
    printf("All json_parser tests passed.\n");
    return 0;
}