#include "sock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* For testing */
#ifndef HTTP_MAIN
#   define HTTP_MAIN http_main
#endif

static void http_server(sock_t *socket, bool plaintext) {
    if (plaintext)
        sock_sendf(socket, "Content-Type: text/plain;charset=us-ascii\r\n");
    sock_sendf(socket, "Server: Redroid HTTP server\r\n\r\n");
}

static void http_header(sock_t *socket, bool plaintext) {
    sock_sendf(socket, "HTTP/1.0 200 OK\r\n");
    http_server(socket, plaintext);
}

static bool http_file(sock_t *socket, const char *file) {
    FILE  *fp   = fopen(file, "r");
    char  *line = NULL;
    size_t size = 0;

    if (!fp)
        return false;

    http_header(socket, false);
    while (getline(&line, &size, fp) != EOF)
        sock_sendf(socket, line);

    free(line);
    fclose(fp);

    return true;
}

static bool http_command(sock_t *socket, const char *command) {
    http_header(socket, true);
    sock_sendf(socket, "%s", command);

    return true;
}

int HTTP_MAIN(void) {
    sock_t *socket = sock_create("::listen", "5050", SOCK_RESTART_NIL);

    if (!socket) {
        fprintf(stderr, "failed creating connection\n");
        sock_destroy(socket, NULL);
        return -1;
    }

    for (;;) {
        sock_t *client = sock_accept(socket);
        if (!client)
            continue; /* no new clients */

        char request[500];
        memset(request, 0, sizeof(request));
        sock_recv(client, request, sizeof(request));

        if (!strncmp(request, "GET /", 5)) {
            /* GET */
            char *file  = &request[5];
            char *strip = strstr(file, " HTTP");

            if (!strip) {
                sock_destroy(client, NULL);
                continue;
            }

            strip[0] = '\0';
            file     = *file ? file : "admin.html";

            /*
             * When there is no extension we consider it a command with
             * a payload.
             */
            (!strchr(file, '.')
                ? &http_command
                : &http_file)(client, file);

        } else if (!strncmp(request, "POST /", 6)) {
            /* POST */
            char *file    = &request[6];
            char *content = strstr(file, "\r\n\r\n");

            content += 4;

            if (!strncmp(content, "login_submit", 12)) {
                /* TODO: deal with login */
            } else if (!strncmp(content, "instance_submit", 15)) {
                /* TODO: deal with instance payload submit */
            } else if (!strncmp(content, "global_submit", 13)) {
                /* TODO: deal with global payload submit */
            } else {
                /* TODO: redirect */
            }
        }
        sock_destroy(client, NULL);
    }
    sock_destroy(socket, NULL);
    return 0;
}
