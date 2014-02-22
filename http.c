#include "http.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

struct http_handle_s {
    void       *data;
    const char *match;
    void      (*callback)(sock_t *client, list_t *kvs, void *data);
};

struct http_s {
    sock_t *host;
    list_t *clients;
    list_t *handles;
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
    ch = tolower(ch);
    static char table[] = "0123456789abcdef";
    return table[ch & 15];
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
typedef struct {
    char *key;
    char *value;
} http_post_kv_t;

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

static bool http_post_kv_search(const void *a, const void *b) {
    const http_post_kv_t *const ka = a;
    return !strcmp(ka->key, (const char *)b);
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
    http_post_kv_t *value;
    while ((value = list_pop(values)))
        http_post_kv_destroy(value);
    list_destroy(values);
}

const char *http_post_find(list_t *values, const char *key) {
    http_post_kv_t *kv = list_search(values, &http_post_kv_search, key);
    return kv ? kv->value : NULL;
}

/* HTTP */
http_t *http_create(const char *port) {
    http_t *http = malloc(sizeof(*http));

    if (!(http->host = sock_create("::listen", port, SOCK_RESTART_NIL))) {
        free(http);
        return NULL;
    }

    http->clients = list_create();
    http->handles = list_create();
    return http;
}

void http_handles_register(http_t *http, const char *match, void (*callback)(sock_t *client, list_t *kvs, void *data), void *data) {
    http_handle_t *handle = malloc(sizeof(*handle));

    handle->match    = match;
    handle->callback = callback;
    handle->data     = data;

    list_push(http->handles, handle);
}

static bool http_handles_search(const void *a, const void *b) {
    const http_handle_t *const ha = a;
    return !strcmp(ha->match, (const char *)b);
}

static void http_handles_destroy(http_t *http) {
    http_handle_t *handle;
    while ((handle = list_pop(http->handles)))
        free(handle);
}

static void http_clients_destroy(http_t *http) {
    sock_t *client;
    while ((client = list_pop(http->clients)))
        sock_destroy(client, NULL);
}

void http_destroy(http_t *http) {
    http_clients_destroy(http);
    http_handles_destroy(http);

    sock_destroy(http->host, NULL);

    list_destroy(http->clients);
    list_destroy(http->handles);

    free(http);
}

/* HTTP send */
static void http_send_header(sock_t *client, size_t length, const char *type) {
    sock_sendf(client, "HTTP/1.1 200 OK\n");
    sock_sendf(client, "Server: Redroid HTTP\n");
    sock_sendf(client, "Content-Length: %zu\n", length);
    sock_sendf(client, "Content-Type: %s\n", type);
    sock_sendf(client, "Connection: close\r\n\r\n");
}

void http_send_plain(sock_t *client, const char *plain) {
    size_t length = strlen(plain);
    http_send_header(client, length, "text/plain");
    sock_send(client, plain, length);
}

void http_send_redirect(sock_t *client, const char *where) {
    sock_sendf(client, "HTTP/1.1 301 Moved Permanently\n");
    sock_sendf(client, "Server: Redroid HTTP\n");
    sock_sendf(client, "Location: %s", where);
}

void http_send_file(sock_t *client, const char *file) {
    FILE   *fp    = fopen(file, "r");
    char   *data  = NULL;
    size_t  size  = 0;

    if (!fp)
        return http_send_plain(client, "404 File not found");

    fseek(fp, 0, SEEK_END);
    size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* Send the header */
    const char *mimetype = http_mime(strrchr(file, '.') + 1);
    http_send_header(client, length, mimetype);

    /* Send contents oneline at a time */
    while (getline(&data, &size, fp) != EOF)
        sock_send(client, data, strlen(data));

    fclose(fp);
    free(data);
}

/* HTTP client management */
static void http_client_accept(http_t *http) {
    sock_t *client;
    if (!(client = sock_accept(http->host)))
        return;

    list_push(http->clients, client);
}

static void http_client_process(list_t *handles, sock_t *client) {
    char buffer[HTTP_BUFFERSIZE];
    int  count;

    /* Ignore nothing */
    if ((count = sock_recv(client, buffer, sizeof(buffer))) <= 0)
        return;

    /* Terminate */
    buffer[count] = '\0';

    /* Handle GET */
    if (!strncmp(buffer, "GET /", 5)) {
        char *file_beg = &buffer[5];
        char *file_end = strchr(file_beg, ' ');

        if (!file_end)
            return;

        *file_end = '\0';
        http_send_file(client, file_beg);
    } else if (!strncmp(buffer, "POST /", 6)) {
        char *post_beg  = &buffer[6];
        char *post_end  = strchr(post_beg, ' ');
        char *post_chop = &post_end[1];

        if (!post_end)
            return;

        /* Terminate to get the right data */
        *post_end = '\0';

        /* Find the data for the POST */
        char *post_data = strstr(post_chop, "\r\n\r\n");
        if (post_data)
            post_data += 4;

        /* Find the post handler */
        http_handle_t *handle = list_search(handles, &http_handles_search, post_beg);
        if (!handle) {
            http_send_plain(client, post_beg);
            return;
        }

        list_t *postdata = http_post_extract(post_data);
        list_t *safedata = list_copy(postdata);
        handle->callback(client, safedata, handle->data);
        http_post_cleanup(postdata);
        list_destroy(safedata);
    }
}

void http_process(http_t *http) {
    http_client_accept(http);

    /* Process clients */
    list_iterator_t *it = list_iterator_create(http->clients);
    while (!list_iterator_end(it)) {
        sock_t *client = list_iterator_next(it);
        http_client_process(http->handles, client);
    }
    list_iterator_destroy(it);

    /* Destroy clients after all were processed */
    http_clients_destroy(http);
}
