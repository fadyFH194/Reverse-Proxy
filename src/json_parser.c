#include "json_parser.h"
#include <string.h>
#include <stdio.h>

static int extract_json_string(const char *json, const char *key, char *out, size_t out_size) {
    char key_str[256];
    snprintf(key_str, sizeof(key_str), "\"%s\"", key);
    char *start = strstr((char*)json, key_str);
    if (!start) return -1;
    start = strchr(start, ':');
    if (!start) return -1;

    // Move past colon and spaces
    while (*start && (*start == ':' || *start == ' ' || *start == '\t'))
        start++;

    if (*start == '\"') start++;

    char *end = strchr(start, '\"');
    if (!end) return -1;
    size_t len = end - start;
    if (len + 1 > out_size) return -1;
    strncpy(out, start, len);
    out[len] = '\0';
    return 0;
}

static int extract_json_int(const char *json, const char *key, int *val) {
    char key_str[256];
    snprintf(key_str, sizeof(key_str), "\"%s\"", key);
    char *start = strstr((char*)json, key_str);
    if (!start) return -1;
    start = strchr(start, ':');
    if (!start) return -1;

    // Move past colon and spaces
    while (*start && (*start == ':' || *start == ' ' || *start == '\t'))
        start++;

    if (sscanf(start, "%d", val) != 1)
        return -1;
    return 0;
}

int parse_json_for_route(const char *json, char *prefix, size_t prefix_size, char *host, size_t host_size, int *port) {
    if (extract_json_string(json, "prefix", prefix, prefix_size) < 0) return -1;
    if (extract_json_string(json, "host", host, host_size) < 0) return -1;
    if (extract_json_int(json, "port", port) < 0) return -1;

    return 0;
}
