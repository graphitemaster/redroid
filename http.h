#ifndef REDROID_HTTP_HDR
#define REDROID_HTTP_HDR
#include "sock.h"
#include "list.h"

typedef struct http_s http_t;
typedef struct http_handle_s http_handle_t;

http_t *http_create(const char *port);
void http_destroy(http_t *http);
void http_handles_register(http_t *http, const char *match, void (*callback)(sock_t *client, list_t *kvs, void *data), void *data);

void http_send_file(sock_t *client, const char *file);
void http_send_plain(sock_t *client, const char *data);
void http_send_redirect(sock_t *client, const char *where);

const char *http_post_find(list_t *values, const char *key);

void http_process(http_t *http);

#endif
