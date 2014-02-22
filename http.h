#ifndef REDROID_HTTP_HDR
#define REDROID_HTTP_HDR
#include <stdbool.h>

#include "sock.h"
#include "list.h"

typedef struct http_s http_t;

http_t *http_create(const char *port);
void http_destroy(http_t *http);
void http_intercept_post(http_t *http, const char *match, void (*callback)(sock_t *client, list_t *kvs, void *data), void *data);
void http_intercept_get(http_t *http, const char *match, void (*predicate)(sock_t *client, void *data), void *data);

void http_send_file(sock_t *client, const char *file);
void http_send_plain(sock_t *client, const char *data);
void http_send_redirect(sock_t *client, const char *where);
void http_send_html(sock_t *client, const char *html);
void http_send_unimplemented(sock_t *client);

const char *http_post_find(list_t *values, const char *key);

void http_process(http_t *http);

#endif
