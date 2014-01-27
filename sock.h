#ifndef REDROID_SOCK_HDR
#define REDROID_SOCK_HDR
#include <stddef.h>
#include <stdbool.h>

typedef int (*sock_send_func)(void *, const char *, size_t);
typedef int (*sock_recv_func)(void *, char *, size_t);
typedef int (*sock_getfd_func)(void *);
typedef bool (*sock_destroy_func)(void *);

typedef struct {
    void             *data;
    sock_send_func    send;
    sock_recv_func    recv;
    sock_getfd_func   getfd;
    sock_destroy_func destroy;
} sock_t;

sock_t *sock_create(const char *host, const char *port, bool ssl);
int sock_send(sock_t *socket, const char *message, size_t size);
int sock_sendf(sock_t *socket, const char *format, ...);
int sock_recv(sock_t *socket, char *buffer, size_t buffersize);
bool sock_destroy(sock_t *socket);
int sock_getfd(sock_t *socket);
void sock_nonblock(int fd);

#endif
