#include "sock.h"
#include "ssl.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

void sock_nonblock(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

static int sock_connection(const char *host, const char *port) {
    int status;
    int sock;

    struct addrinfo *result;
    struct addrinfo  hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    if ((status = getaddrinfo(host, port, &hints, &result)) < 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    if ((sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) < 0) {
        fprintf(stderr, "failed to get socket: %s:%s\n", host, port);
        goto sock_get_error;
    }

    // try all of them until it succeeds
    bool failed = false;
    for (struct addrinfo *current = result; current; current = current->ai_next) {
        char ipbuffer[INET6_ADDRSTRLEN];
        void *addr = NULL;
        if (current->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in*)current->ai_addr;
            addr = &(ipv4->sin_addr);
        } else {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)current->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        inet_ntop(current->ai_family, addr, ipbuffer, sizeof(ipbuffer));

        if ((status = connect(sock, result->ai_addr, result->ai_addrlen)) < 0) {
            if (current->ai_next)
                fprintf(stderr, "failed to connect: %s:%s (trying next address in list)\n", host, ipbuffer);
            else
                fprintf(stderr, "failed to connect: %s:%s %s\n", host, ipbuffer, strerror(errno));
            failed = true;
        } else {
            result = current;
            failed = false;
            break;
        }
    }
    if (failed)
        goto sock_get_error;

    freeaddrinfo(result);
    return sock;

sock_get_error:
    freeaddrinfo(result);
    return -1;
}

// standard non SSL sockets
typedef struct {
    int fd;
} sock_ctx_t;

static int sock_standard_recv(sock_ctx_t *ctx, char *buffer, size_t size) {
    int ret;
    if ((ret = recv(ctx->fd, buffer, size, 0)) <= 0)
        return -1;
    return ret;
}

static int sock_standard_send(sock_ctx_t *ctx, const char *data, size_t size) {
    size_t written = 0;
    int    read;

    while (written < size) {
        if ((read = send(ctx->fd, data + written, size - written, 0)) <= 0)
            return -1;
        written += read;
    }

    return (int)written;
}

static bool sock_standard_destroy(sock_ctx_t *ctx, sock_restart_t *restart) {
    if (restart) {
        restart->fd   = ctx->fd;
        restart->ssl  = false;
        restart->data = NULL;
        restart->size = 0;

        free(ctx);
        return true;
    }

    bool succeed = (close(ctx->fd) == 0);
    free(ctx);
    return succeed;
}

static int sock_standard_getfd(sock_ctx_t *ctx) {
    return ctx->fd;
}

static sock_t *sock_standard_create(int fd) {
    sock_t     *socket = malloc(sizeof(*socket));
    sock_ctx_t *data   = malloc(sizeof(*data));

    data->fd        = fd;
    socket->data    = data;
    socket->getfd   = (sock_getfd_func)&sock_standard_getfd;
    socket->send    = (sock_send_func)&sock_standard_send;
    socket->recv    = (sock_recv_func)&sock_standard_recv;
    socket->destroy = (sock_destroy_func)&sock_standard_destroy;
    socket->ssl     = false;

    sock_nonblock(fd);
    return socket;
}

// exposed interface
sock_t *sock_create(const char *host, const char *port, sock_restart_t *restart) {
    if (restart->fd != -1) {
#ifdef HAS_SSL
        if (restart->ssl)
            return ssl_create(restart->fd, restart);
#endif
        return sock_standard_create(restart->fd);
    }

    int fd = sock_connection(host, port);
    if (fd == -1)
        return NULL;

#ifdef HAS_SSL
    if (restart->ssl)
        return ssl_create(fd, NULL);
#endif

    return sock_standard_create(fd);
}

int sock_sendf(sock_t *socket, const char *fmt, ...) {
    char    buffer[512];
    int     length;
    va_list args;

    if (!strlen(fmt))
        return 0;

    va_start(args, fmt);
    length = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (length > sizeof(buffer))
        raise(SIGUSR1);

    if (socket->send(socket->data, buffer, length) <= 0)
        return -1;

    return length;
}

int sock_recv(sock_t *socket, char *buffer, size_t buffersize) {
    return socket->recv(socket->data, buffer, buffersize);
}

int sock_send(sock_t *socket, const char *message, size_t size) {
    return socket->send(socket->data, message, size);
}

int sock_getfd(sock_t *socket) {
    if (!socket)
        return -1;
    return socket->getfd(socket->data);
}


static void sock_restart_dump(sock_restart_t *restart) {
#ifndef _NDEBUG
    unsigned char buffer[17] = "";
    size_t i;
    for (i = 0; i < restart->size; i++) {
        if (i % 16 == 0) {
            if (i != 0)
                printf("  %s\n", buffer);
            printf("  %04x ", i);
        }

        printf(" %02x", restart->data[i]);

        if (restart->data[i] < 0x20 || restart->data[i] > 0x7E)
            buffer[i % 16] = '.';
        else
            buffer[i % 16] = restart->data[i];
        buffer[(i % 16) + 1] = 0;
    }

    while ((i % 16) != 0)
        printf("   "), i++;
    if (*buffer)
        printf("  %s\n", buffer);
#endif
}

bool sock_destroy(sock_t *socket, sock_restart_t *restart) {
    if (!socket)
        return false;

    bool succeed = false;
    socket->destroy(socket->data, restart);

    if (restart && restart->fd != -1)
        sock_restart_dump(restart);

    free(socket);
    return succeed;
}
