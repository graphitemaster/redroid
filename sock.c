#include "sock.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

int sock_get(const char *host, const char *port) {
    int status;
    int sock;

    struct addrinfo *result;
    struct addrinfo  hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags    = AI_PASSIVE
    };

    if ((status = getaddrinfo(host, port, &hints, &result)) < 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    if ((sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) < 0) {
        fprintf(stderr, "failed to get socket: %s:%s\n", host, port);
        goto sock_get_error;
    }

    if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
        fprintf(stderr, "failed to connect: %s:%s\n", host, port);
        goto sock_get_error;
    }

    freeaddrinfo(result);
    //
    // Make the socket non-blocking:
    //  On shutdown we could block in a sock_recv(). Waiting for
    //  all servers to send their final message (typically a PING)
    //  could take too long.
    //
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    return sock;

sock_get_error:
    freeaddrinfo(result);
    return -1;
}

int sock_send(int sock, const char *data, size_t size) {
    size_t written = 0;
    int    read;

    while (written < size) {
        if ((read = send(sock, data + written, size - written, 0)) <= 0)
            return -1;

        written += read;
    }

    return (int)written;
}

int sock_sendf(int sock, const char *fmt, ...) {
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

    if (sock_send(sock, buffer, length) <= 0)
        return -1;

    return length;
}

int sock_recv(int sock, char *buffer, size_t size) {
    int ret;
    if ((ret = recv(sock, buffer, size, 0)) <= 0)
        return -1;
    return ret;
}

int sock_close(int sock) {
    return close(sock);
}
