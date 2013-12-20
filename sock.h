#ifndef REDROID_SOCK_HDR
#define REDROID_SOCK_HDR
#include <stddef.h> /* size_t */

int sock_get(const char *host, const char *port);
int sock_send(int sock, const char *data, size_t size);
int sock_sendf(int sock, const char *fmt, ...);
int sock_recv(int sock, char *buffer, size_t size);
int sock_close(int sock);

#endif
