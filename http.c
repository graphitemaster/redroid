#include "http.h"
#include "string.h"

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>

void redroid_abort(void);

typedef struct {
    bool  post;
    void *data;
    char *match;
    union {
        void (*post)(sock_t *client, list_t *kvs, void *data);
        void (*get)(sock_t *client, void *data);
    } callback;
} http_intercept_t;

typedef struct {
    size_t  refs;
    sock_t *sock;
    time_t  time;
} http_client_t;

struct http_s {
    sock_t       *host;
    list_t       *clients;
    list_t       *intercepts;
    struct pollfd polls[2];
    int           wakefds[2];
};

#define HTTP_BUFFERSIZE 8096

/* MIME */
static struct {
    const char *extension;
    const char *mimetype;
} const http_mimetypes[] = {
    { "html", "text/html"  },
    { "htm",  "text/htm"   },
    { "css",  "text/css"   },
    { "gif",  "image/gif"  },
    { "jpg",  "image/jpg"  },
    { "jpeg", "image/jpeg" },
    { "png",  "image/png"  }
};

static const char *http_mime(const char *extension) {
    for (size_t i = 0; i < sizeof(http_mimetypes)/sizeof(*http_mimetypes); i++)
        if (!strcmp(http_mimetypes[i].extension, extension))
            return http_mimetypes[i].mimetype;
    return "text/plain";
}

/* URL */
static char http_url_fromhex(char ch) {
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

static char *http_url_decode(const char *str) {
    const char *tok = str;
    char       *beg = malloc(strlen(str) + 1);
    char       *end = beg;

    while (*tok) {
        if (*tok == '%') {
            if (tok[1] && tok[2]) {
                *end++ = http_url_fromhex(tok[1]) << 4 | http_url_fromhex(tok[2]);
                tok += 2;
            }
        } else {
            *end++ = (*tok == '+') ? ' ' : *tok;
        }
        tok++;
    }
    *end = '\0';
    return beg;
}

/* POST keyvalue parser */
static http_post_kv_t *http_post_kv_create(const char *key, const char *value) {
    http_post_kv_t *kv = malloc(sizeof(*kv));
    kv->key   = http_url_decode(key);
    kv->value = http_url_decode(value);

    return kv;
}

static void http_post_kv_destroy(http_post_kv_t *value) {
    free(value->key);
    free(value->value);
    free(value);
}

static list_t *http_post_extract(char *content) {
    list_t *list = list_create();

    char *tok = strtok(content, "&");
    while (tok) {
        char *key   = tok;
        char *value = strchr(key, '=');

        *value = '\0';
        value++;

        list_push(list, http_post_kv_create(key, value));
        tok = strtok(NULL, "&");
    }

    return list;
}

static void http_post_cleanup(list_t *values) {
    list_foreach(values, NULL, &http_post_kv_destroy);
    list_destroy(values);
}

const char *http_post_find(list_t *values, const char *key) {
    http_post_kv_t *kv = list_search(values, key,
        lambda bool(const http_post_kv_t *a, const char *b)
            => return !strcmp(a->key, b);
    );
    return kv ? kv->value : NULL;
}

/* HTTP */
http_t *http_create(const char *port) {
    http_t *http = malloc(sizeof(*http));

    if (!(http->host = sock_create("::listen", port, SOCK_RESTART_NIL))) {
        printf("error %s\n", strerror(errno));
        free(http);
        return NULL;
    }

    if (pipe(http->wakefds) == -1) {
        free(http);
        return NULL;
    }

    http->polls[0].fd     = http->wakefds[0];
    http->polls[0].events = POLLIN | POLLPRI;

    int flags = fcntl(http->wakefds[0], F_GETFL);
    if (flags == -1)
        goto http_create_error;
    flags |= O_NONBLOCK;
    if (fcntl(http->wakefds[0], F_SETFL, flags) == -1)
        goto http_create_error;

    flags = fcntl(http->wakefds[1], F_GETFL);
    if (flags == -1)
        goto http_create_error;
    flags |= O_NONBLOCK;
    if (fcntl(http->wakefds[1], F_SETFL, flags) == -1)
        goto http_create_error;

    http->polls[1].fd     = sock_getfd(http->host);
    http->polls[1].events = POLLIN | POLLPRI;

    http->clients    = list_create();
    http->intercepts = list_create();

    /*
     * Ignore SIGPIPE because it's possible to write to a broken socket if the
     * peer disconnects before everything is sent.
     */
    signal(SIGPIPE, SIG_IGN);

    return http;

http_create_error:
    close(http->wakefds[0]);
    close(http->wakefds[1]);

    sock_destroy(http->host, NULL);
    free(http);

    return NULL;
}

void http_intercept_post(
    http_t     *http,
    const char *match,
    void      (*callback)(sock_t *client, list_t *kvs, void *data),
    void       *data
) {
    http_intercept_t *intercept = malloc(sizeof(*intercept));

    intercept->post          = true;
    intercept->match         = strdup(match);
    intercept->callback.post = callback;
    intercept->data          = data;

    list_push(http->intercepts, intercept);
}

void http_intercept_get(
    http_t     *http,
    const char *match,
    void      (*callback)(sock_t *client, void *data),
    void       *data
) {
    http_intercept_t *intercept = malloc(sizeof(*intercept));

    intercept->post         = false;
    intercept->match        = strdup(match);
    intercept->callback.get = callback;
    intercept->data         = data;

    list_push(http->intercepts, intercept);
}

static void http_intercepts_destroy(http_t *http) {
    http_intercept_t *handle;
    while ((handle = list_pop(http->intercepts))) {
        free(handle->match);
        free(handle);
    }
}

/* HTTP send */
static void http_send_header(sock_t *client, size_t length, const char *type) {
    sock_sendf(client, "HTTP/1.1 200 OK\n");
    sock_sendf(client, "Server: Redroid HTTP\n");
    sock_sendf(client, "Content-Length: %zu\n", length);
    sock_sendf(client, "Content-Type: %s\n", type);
    sock_sendf(client, "Connection: close\r\n\r\n");
}

void http_send_unimplemented(sock_t *client) {
    sock_sendf(client, "HTTP/1.1 501 Not Implemented\n");
    sock_sendf(client, "Server: Redroid HTTP\r\n\r\n");
}

void http_send_error(sock_t *client) {
    sock_sendf(client, "HTTP/1.1 500 Interanl Server Error\n");
    sock_sendf(client, "Server: Redroid HTTP\r\n\r\n");
}

void http_send_404(sock_t *client) {
    sock_sendf(client, "HTTP/1.1 404 File Not Found\n");
    sock_sendf(client, "Server: Redroid HTTP\r\n\r\n");
}

void http_send_html(sock_t *client, const char *html) {
    size_t length = strlen(html);
    http_send_header(client, length, "text/html");
    sock_send(client, html, length);
}

void http_send_plain(sock_t *client, const char *plain) {
    size_t length = strlen(plain);
    http_send_header(client, length, "text/plain");
    sock_send(client, plain, length);
}

void http_send_redirect(sock_t *client, const char *where) {
    sock_sendf(client, "HTTP/1.1 301 Moved Permanently\n");
    sock_sendf(client, "Server: Redroid HTTP\n");
    sock_sendf(client, "Connection: close\n");
    sock_sendf(client, "Location: %s\r\n\r\n", where);
}

void http_send_file(sock_t *client, const char *file) {
    /* Default to index.html for empty file */
    if (!*file)
        file = "index.html";

    string_t *mount = string_format("site/%s", file);
    FILE     *fp    = fopen(string_contents(mount), "r");
    char     *data  = NULL;
    size_t    size  = 0;

    string_destroy(mount);
    if (!fp)
        return http_send_404(client);

    fseek(fp, 0, SEEK_END);
    size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    const char *extension = strrchr(file, '.');
    if (!extension) {
        fclose(fp);
        return http_send_error(client);
    }
    const char *mimetype = http_mime(extension + 1);
    http_send_header(client, length, mimetype);

    while (getline(&data, &size, fp) != EOF)
        sock_send(client, data, strlen(data));

    fclose(fp);
    free(data);
}

/* HTTP client management */
static http_client_t *http_client_create(http_t *http, sock_t *client) {
    http_client_t *find = list_search(http->clients, client,
        lambda bool(const http_client_t *client, const sock_t *sock)
            => return sock_getfd(client->sock) == sock_getfd(sock);
    );
    if (find) {
        sock_destroy(client, NULL);
        find->refs++;
        return find;
    }

    find        = malloc(sizeof(*find));
    find->refs  = 1;
    find->sock  = client;
    find->time  = time(0);

    return find;
}

static void http_client_destroy(http_t *http, http_client_t *client) {
    if (--client->refs != 0)
        return;

    sock_destroy(client->sock, NULL);

    list_erase(http->clients, client);

    free(client);
}

static void http_client_accept(http_t *http) {
    sock_t *client;
    if (!(client = sock_accept(http->host)))
        return;

    list_push(http->clients, http_client_create(http, client));
}

static void http_client_process(http_t *http, sock_t *client) {
    char buffer[HTTP_BUFFERSIZE];
    int  count;

    if ((count = sock_recv(client, buffer, sizeof(buffer))) <= 0)
        return;

    buffer[count] = '\0';

    bool(*search)(const http_intercept_t *const intercept, const char *match) =
        lambda bool(const http_intercept_t *const intercept, const char *match)
            => return !strcmp(intercept->match, match);;

    if (!strncmp(buffer, "GET /", 5)) {
        char *file_beg = &buffer[5];
        char *file_end = strchr(file_beg, ' ');

        if (!file_end)
            return;

        *file_end = '\0';
        http_intercept_t *predicate = list_search(http->intercepts, file_beg, search);
        if (!predicate || predicate->post)
            return http_send_file(client, file_beg);

        predicate->callback.get(client, predicate->data);
    } else if (!strncmp(buffer, "POST /", 6)) {
        char *post_beg  = &buffer[6];
        char *post_end  = strchr(post_beg, ' ');
        char *post_chop = &post_end[1];

        if (!post_end)
            return;

        *post_end = '\0';

        /* Find the data for the POST */
        char *post_data = strstr(post_chop, "\r\n\r\n");
        if (post_data)
            post_data += 4;

        http_intercept_t *intercept = list_search(http->intercepts, post_beg, search);
        if (!intercept || !intercept->post)
            return http_send_plain(client, post_beg);

        list_t *postdata = http_post_extract(post_data);
        list_t *safedata = list_copy(postdata);
        intercept->callback.post(client, safedata, intercept->data);
        http_post_cleanup(postdata);
        list_destroy(safedata);
    } else {
        http_send_unimplemented(client);
    }
}

void http_process(http_t *http) {
    int wait = poll(http->polls, 2, -1);
    if (wait == 0 || wait == -1)
        return;

    if (http->polls[0].revents & POLLIN) {
        char data[6];
        read(http->wakefds[0], data, sizeof(data));
        return;
    }

    http_client_accept(http);

    list_foreach(http->clients, http,
        lambda void(http_client_t *client, http_t *http)
            => http_client_process(http, client->sock);
    );

    list_foreach(http->clients, http,
        lambda void(http_client_t *client, http_t *http) {
            if (difftime(client->time, time(0)) >= 60 * 2)
                http_client_destroy(http, client);
        }
    );
}

static void http_terminate(http_t *http) {
    write(http->wakefds[1], "wakeup", 6);
}

void http_destroy(http_t *http) {
    http_terminate(http);
    http_intercepts_destroy(http);
    sock_destroy(http->host, NULL);

    list_foreach(http->clients, NULL, &http_client_destroy);
    list_destroy(http->clients);
    list_destroy(http->intercepts);

    close(http->wakefds[0]);
    close(http->wakefds[1]);

    free(http);
}
