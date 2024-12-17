#ifndef PROXY_H
#define PROXY_H

int start_proxy(int port, void *(*handler)(void *));
void *handle_client_request(void *arg);  // Updated return type to match implementation

#endif
