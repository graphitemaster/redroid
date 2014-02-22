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

bool sock_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        return false;

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1)
        return false;

    return true;
}

static int sock_listen(const char *port) {
    int sock;
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        return -1;

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1)
        goto sock_listen_error;

    struct sockaddr_in host = {
        .sin_family      = AF_INET,
        .sin_port        = htons(atoi(port)),
        .sin_addr.s_addr = INADDR_ANY
    };

    memset(&host.sin_zero, 0, 8);

    /*
     * Try binding the listen socket connection now. If this fails the
     * port is likely in use.
     */
    if (bind(sock, (struct sockaddr *)&host, sizeof(struct sockaddr)) == -1)
        goto sock_listen_error;

    /*
     * Begin listening for connections now.
     *  We'll limit ourselfs to a total of 16 backlogged for established
     *  sockets to be accepted.
     */
    if (listen(sock, 16) == -1)
        goto sock_listen_error;

    return sock;

sock_listen_error:
    close(sock);
    return -1;
}

static int sock_connection(const char *host, const char *port, char **resolved) {
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
            result    = current;
            failed    = false;
            *resolved = strdup(ipbuffer);
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

static sock_t *sock_standard_create(int fd, bool listen, const char *host) {
    sock_t     *socket = malloc(sizeof(*socket));
    sock_ctx_t *data   = malloc(sizeof(*data));

    data->fd        = fd;
    socket->data    = data;
    socket->getfd   = (sock_getfd_func)&sock_standard_getfd;
    socket->send    = (sock_send_func)&sock_standard_send;
    socket->recv    = (sock_recv_func)&sock_standard_recv;
    socket->destroy = (sock_destroy_func)&sock_standard_destroy;
    socket->ssl     = false;
    socket->listen  = listen;
    socket->host    = strdup(host);

    if (!sock_nonblock(fd)) {
        free(socket);
        free(data);
        return NULL;
    }

    return socket;
}

// exposed interface
sock_t *sock_create(const char *host, const char *port, sock_restart_t *restart) {
    bool listen = !strcmp(host, "::listen");

    /* Listen servers can be restarted as well */
    if (restart->fd != -1) {
#ifdef HAS_SSL
        if (restart->ssl)
            return ssl_create(restart->fd, restart);
#endif
        /*
         * We'll need to re-resolve the peer name as the restart file
         * doesn't contain that information.
         */
        struct sockaddr_storage addr;
        socklen_t               size = sizeof(addr);
        char                    resolved[INET6_ADDRSTRLEN];

        /*
         * Can't resolve after restart means the client or the host is
         * without internet.
         */
        if (getpeername(restart->fd, (struct sockaddr*)&addr, &size) == -1)
            return NULL;


        if (addr.ss_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in*)&addr;
            inet_ntop(AF_INET, &ipv4->sin_addr, resolved, sizeof(resolved));
        } else {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)&addr;
            inet_ntop(AF_INET6, &ipv6->sin6_addr, resolved, sizeof(resolved));
        }

        return sock_standard_create(restart->fd, true, resolved);
    }

    /*
     * Listen sockets act a little different than connection sockets.
     * We make those discinctions here.
     */
    int fd = -1;
    if (listen) {
        fd = sock_listen(port);
        if (fd == -1)
            return NULL;

        /* Listen servers cannot SSL */
        return sock_standard_create(fd, true, host);
    }

    char *resolved = NULL;
    fd = sock_connection(host, port, &resolved);
    if (fd == -1)
        return NULL;

    sock_t *sock = sock_standard_create(fd, false, resolved);
    free(resolved);
    return sock;
}

int sock_sendf(sock_t *socket, const char *fmt, ...) {
    char    buffer[4096];
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

sock_t *sock_accept(sock_t *socket) {
    /* Can only accept on listening sockets */
    if (!socket->listen)
        return NULL;

    struct sockaddr_in clientaddr;
    struct sockaddr   *client = (struct sockaddr *)&clientaddr;

    int hostfd   = sock_getfd(socket);
    int clientfd = accept(hostfd, client, &(socklen_t){sizeof(struct sockaddr_in)});

    if (clientfd == -1)
        return NULL;

    /* Resolved client host is saved for sessions */
    return sock_standard_create(clientfd, false, inet_ntoa(clientaddr.sin_addr));
}

bool sock_destroy(sock_t *socket, sock_restart_t *restart) {
    if (!socket)
        return false;

    bool succeed = false;
    socket->destroy(socket->data, restart);
    free(socket->host);
    free(socket);
    return succeed;
}
