#ifndef REDROID_SSL_HDR
#define REDROID_SSL_HDR
#include "sock.h"

sock_t *ssl_create(int fd, sock_restart_t *restart);

#endif
