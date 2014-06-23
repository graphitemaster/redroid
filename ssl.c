#ifdef HAS_SSL

#include <stdio.h>
#include <stdlib.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "sock.h"

#define CAFILE "/etc/ssl/certs/ca-certificates.crt"

static int ssl_instances = 0;

typedef struct {
    int                              fd;
    gnutls_session_t                 session;
    gnutls_certificate_credentials_t xcred;
} ssl_t;

static const char *ssl_certificate_serial(const void *bin, size_t size) {
    static char memory[110];
    char *ptr = memory;
    if (size > 50)
        size = 50;
    for (size_t i = 0; i < size; i++, ptr += 2)
        sprintf(ptr, "%.2x ", ((unsigned char *)bin)[i]);
    return memory;
}

static int ssl_certificate_check(gnutls_session_t session) {
    const char    *hostname = gnutls_session_get_ptr(session);
    unsigned int   status   = 0;
    int            verify   = gnutls_certificate_verify_peers3(session, hostname, &status);

    if (verify < 0) {
        fprintf(stderr, "    ssl      => certificate verification failed\n");
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    if (status != 0) {
        fprintf(stderr, "    ssl      => certificate isn't trusted\n");
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    /* get certififcate chain */
    unsigned int          certlist = 0;
    const gnutls_datum_t *certdata = gnutls_certificate_get_peers(session, &certlist);

    printf("    ssl      => certificate chain\n");
    for (size_t i = 0; i < certlist; i++) {
        gnutls_x509_crt_t cert;
        gnutls_x509_crt_init(&cert);
        gnutls_x509_crt_import(cert, &certdata[i], GNUTLS_X509_FMT_DER);

        /* get distinguished name */
        char dndata[256];
        gnutls_x509_crt_get_dn(cert, dndata, &(size_t){ sizeof(dndata) });

        /* get serial */
        char serialdata[40];
        size_t serialsize = sizeof(serialdata);
        gnutls_x509_crt_get_serial(cert, serialdata, &serialsize);
        const char *serial = ssl_certificate_serial(serialdata, serialsize);

        /* get certificate times */
        time_t expiration_time = gnutls_x509_crt_get_expiration_time(cert);
        time_t activation_time = gnutls_x509_crt_get_activation_time(cert);

        /* get public key choice */
        unsigned int bits = 0;
        unsigned int algo = gnutls_x509_crt_get_pk_algorithm(cert, &bits);

        char issuerdata[256];
        gnutls_x509_crt_get_issuer_dn(cert, issuerdata, &(size_t) { sizeof(issuerdata) });

        printf("    ssl      =>     %s\n", dndata);
        printf("    ssl      =>         serial    %s\n", serial);
        printf("    ssl      =>         activated %s", ctime(&activation_time));
        printf("    ssl      =>         expires   %s", ctime(&expiration_time));
        printf("    ssl      =>         algorithm %s (%u bits)\n", gnutls_pk_algorithm_get_name(algo), bits);
        printf("    ssl      =>         issuer    %s\n", issuerdata);

        gnutls_x509_crt_deinit(cert);
    }

    return 0;
}

static ssl_t *ssl_ctx_create(int fd) {
    if (ssl_instances++ == 0) {
        printf("    ssl      => initialized\n");
        gnutls_global_init();
    }

    ssl_t *ssl = malloc(sizeof(*ssl));
    if (!ssl)
        return NULL;

    ssl->fd = fd;
    if (gnutls_certificate_allocate_credentials(&ssl->xcred) != GNUTLS_E_SUCCESS)
        goto ssl_ctx_error;

    if (gnutls_certificate_set_x509_trust_file(ssl->xcred, CAFILE, GNUTLS_X509_FMT_PEM) <= 0)
        goto ssl_ctx_error;

    gnutls_certificate_set_verify_function(ssl->xcred, ssl_certificate_check);

    if (gnutls_init(&ssl->session, GNUTLS_CLIENT) != GNUTLS_E_SUCCESS)
        goto ssl_ctx_error;

    if (gnutls_set_default_priority(ssl->session) != GNUTLS_E_SUCCESS)
        goto ssl_ctx_error_session;

    if (gnutls_credentials_set(ssl->session, GNUTLS_CRD_CERTIFICATE, ssl->xcred) != GNUTLS_E_SUCCESS)
        goto ssl_ctx_error_session;

    gnutls_transport_set_int(ssl->session, fd);
    gnutls_handshake_set_timeout(ssl->session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);

    /* Do handshake now */
    printf("    ssl      => performing handshake\n");
    int handshake;
    do {
        handshake = gnutls_handshake(ssl->session);
    } while (handshake < 0 && gnutls_error_is_fatal(handshake) == 0);

    if (handshake < 0) {
        fprintf(stderr, "    ssl      => handshake failed\n");
        gnutls_perror(handshake);
        goto ssl_ctx_error_session;
    } else {
        char *session = gnutls_session_get_desc(ssl->session);
        printf("    ssl      => connected via %s\n", session);
        gnutls_free(session);
    }

    return ssl;

ssl_ctx_error_session:
    gnutls_deinit(ssl->session);

ssl_ctx_error:
    gnutls_certificate_free_credentials(ssl->xcred);
    free(ssl);
    return NULL;
}

static bool ssl_destroy(ssl_t *ssl, sock_restart_t *restart) {
    (void)restart;

    int bye;
    do {
        bye = gnutls_bye(ssl->session, GNUTLS_SHUT_RDWR);
    } while (bye == GNUTLS_E_AGAIN);

    gnutls_deinit(ssl->session);
    gnutls_certificate_free_credentials(ssl->xcred);
    free(ssl);

    if (--ssl_instances == 0) {
        gnutls_global_deinit();
        printf("    ssl      => deinitialized\n");
    }

    return true;
}

static int ssl_recv(ssl_t *ssl, char *buffer, size_t size) {
    int ret = gnutls_record_recv(ssl->session, buffer, size);
    if (ret == 0) {
        printf("    ssl      => peer has closed connection\n");
        return 0;
    } else if (ret < 0 && gnutls_error_is_fatal(ret) == 0) {
        /* Non fatal error */
        return 0;
    } else if (ret < 0) {
        /* Fatal error */
        fprintf(stderr, "    ssl      => %s\n", gnutls_strerror(ret));
        return -1;
    }
    return ret;
}

static int ssl_send(ssl_t *ssl, const char *message, size_t size) {
    return gnutls_record_send(ssl->session, message, size);
}

static int ssl_getfd(const ssl_t *ssl) {
    return ssl->fd;
}

sock_t *ssl_create(int fd, sock_restart_t *restart) {
    (void)restart;

    ssl_t *ssl = ssl_ctx_create(fd);
    if (!ssl) {
        fprintf(stderr, "    ssl      => failed creating context\n");
        return NULL;
    }

    sock_t *sock = malloc(sizeof(*sock));
    sock->data    = ssl;
    sock->getfd   = (sock_getfd_func)&ssl_getfd;
    sock->recv    = (sock_recv_func)&ssl_recv;
    sock->send    = (sock_send_func)&ssl_send;
    sock->destroy = (sock_destroy_func)&ssl_destroy;
    sock->ssl     = true;

    return sock;
}

#endif
