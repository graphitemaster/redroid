#ifdef HAS_SSL

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/engine.h>

#include <unistd.h>

#include "ssl.h"

static struct {
    bool   init;
    size_t refcount;
} ssl_global_state = {
    .init     = false,
    .refcount = 0
};

typedef struct {
    int      fd;
    SSL_CTX *ctx;
    SSL     *ssl;
} ssl_t;

static bool ssl_certificate_check(ssl_t *ssl) {
    X509 *cert = SSL_get_peer_certificate(ssl->ssl);
    if (!cert) {
        fprintf(stderr, "    ssl      => no certificate\n");
        return false;
    }

    char *subject = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
    char *issuer  = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);

    printf("    ssl      => Server certificate:\n");
    printf("    ssl      =>     Subject: %s\n", subject);
    printf("    ssl      =>     Issuer:  %s\n", issuer);

    free(subject);
    free(issuer);

    X509_free(cert);

    return true;
}

static ssl_t *ssl_ctx_create(int fd) {
    ssl_t *ssl = malloc(sizeof(*ssl));

    if (!ssl_global_state.init) {
        SSL_library_init();
        ssl_global_state.init = true;
        printf("    ssl      => initialized\n");
    }

    ssl->fd = fd;
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    const SSL_METHOD *method = SSLv23_client_method();

    if (!(ssl->ctx = SSL_CTX_new(method)))
        goto ssl_create_error;
    if (!(ssl->ssl = SSL_new(ssl->ctx)))
        goto ssl_create_error;

    ssl_global_state.refcount++;
    return ssl;

ssl_create_error:
    ERR_print_errors_fp(stderr);
    if (ssl->ctx)
        SSL_CTX_free(ssl->ctx);
    free(ssl);
    return NULL;
}

static bool ssl_destroy(ssl_t *ssl, sock_restart_t *restart) {
    (void)restart; /* ignroed */

    if (ssl->ssl)
        SSL_free(ssl->ssl);

    SSL_CTX_free(ssl->ctx);
    bool succeed = (close(ssl->fd) == 0);
    free(ssl);

    /*
     * Deal with global state of OpenSSL via reference count since there
     * is no other sane way unless we used something like YASSL.
     */
    if (--ssl_global_state.refcount == 0) {
        ERR_remove_state(0);
        ENGINE_cleanup();
        ERR_free_strings();
        EVP_cleanup();
        CRYPTO_cleanup_all_ex_data();
        sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
    }

    return succeed;
}

static int ssl_recv(ssl_t *ssl, char *buffer, size_t size) {
    return SSL_read(ssl->ssl, buffer, size);
}

static int ssl_send(ssl_t *ssl, const char *message, size_t size) {
    return SSL_write(ssl->ssl, message, size);
}

static int ssl_getfd(const ssl_t *ssl) {
    return ssl->fd;
}

sock_t *ssl_create(int fd, sock_restart_t *restart) {
    ssl_t *ssl = ssl_ctx_create(fd);
    if (!ssl) {
        fprintf(stderr, "   ssl      => failed creating context\n");
        return NULL;
    }

    sock_t *sock = malloc(sizeof(*sock));
    sock->data    = ssl;
    sock->getfd   = (sock_getfd_func)&ssl_getfd;
    sock->recv    = (sock_recv_func)&ssl_recv;
    sock->send    = (sock_send_func)&ssl_send;
    sock->destroy = (sock_destroy_func)&ssl_destroy;
    sock->ssl     = true;

    if (!SSL_set_fd(ssl->ssl, fd))
        goto cleanup;

    if (SSL_connect(ssl->ssl) != 1)
        goto cleanup;

    if (!ssl_certificate_check(ssl))
        goto cleanup;

    if (!sock_nonblock(fd))
        goto cleanup;

    printf("    ssl      => connected with %s encryption\n", SSL_get_cipher(ssl->ssl));

    return sock;

cleanup:
    if (restart && restart->data)
        free(restart->data);

    fprintf(stderr, "    ssl      => failed creating SSL\n");
    ERR_print_errors_fp(stderr);
    free(sock);
    ssl_destroy(ssl, NULL);
    return NULL;
}

#endif
