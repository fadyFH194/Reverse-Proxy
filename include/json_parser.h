#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stddef.h>

// Parses JSON of the form:
// {
//   "prefix": "/someprefix",
//   "reqHostNames": [...],
//   "host": "localhost",
//   "port": 4000
// }
// Extracts prefix, host, and port.
//
// Returns 0 on success, -1 on failure.
int parse_json_for_route(const char *json, char *prefix, size_t prefix_size, char *host, size_t host_size, int *port);

#endif
