#include "web.h"
#include "http.h"
#include "mt.h"
#include "ripemd.h"
#include "database.h"
#include "string.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

typedef struct {
    char    *host;
    bool     valid;
} web_session_t;

struct web_s {
    mt_t           *rand;
    ripemd_t       *ripemd;
    database_t     *database;
    http_t         *server;
    pthread_t       thread;
    pthread_mutex_t mutex;
    list_t         *sessions;
};

static web_session_t *web_session_create(web_t *web, sock_t *client) {
    web_session_t *session = malloc(sizeof(*session));
    session->host  = strdup(client->host);
    session->valid = true;
    return session;
}

static void web_session_destroy(web_session_t *session) {
    free(session->host);
    free(session);
}

static bool web_session_search(const void *a, const void *b) {
    const web_session_t *const sa = a;
    const sock_t        *const sb = b;

    return !strcmp(sa->host, sb->host);
}

static void web_session(web_t *web, sock_t *client) {
    /* Check if there isn't already a session for the client first */
    if (list_search(web->sessions, &web_session_search, client))
        return;

    list_push(web->sessions, web_session_create(web, client));
}

static bool web_check_admin(sock_t *client, void *data) {
    web_t         *web     = data;
    web_session_t *session = list_search(web->sessions, &web_session_search, client->host);

    return !!(session && session->valid);
}

static void web_login(sock_t *client, list_t *post, void *data) {
    web_t *web = data;

    const char *username = http_post_find(post, "username");
    const char *password = http_post_find(post, "password");

    database_statement_t *statement = database_statement_create(web->database, "SELECT SALT FROM USERS WHERE USERNAME=?");
    database_row_t       *row       = NULL;

    if (!statement)
        return http_send_file(client, "invalid.html");
    if (!database_statement_bind(statement, "s", username))
        return http_send_file(client, "invalid.html");
    if (!(row = database_row_extract(statement, "s")))
        return http_send_file(client, "invalid.html");

    const char    *getsalt         = database_row_pop_string(row);
    string_t      *saltpassword    = string_format("%s%s", getsalt, password);
    const char    *saltpasswordraw = string_contents(saltpassword);
    unsigned char *ripemdpass      = ripemd_compute(web->ripemd, saltpasswordraw, string_length(saltpassword));
    string_t      *hashpassword    = string_construct();

    string_destroy(saltpassword);
    database_row_destroy(row);

    for (size_t i = 0; i < 160 / 8; i++)
        string_catf(hashpassword, "%02x", ripemdpass[i]);

    statement = database_statement_create(web->database, "SELECT COUNT(*) FROM USERS WHERE USERNAME=? AND PASSWORD=?");

    if (!statement)
        return string_destroy(hashpassword);
    if (!database_statement_bind(statement, "sS", username, hashpassword))
        return string_destroy(hashpassword);
    if (!(row = database_row_extract(statement, "i")))
        return string_destroy(hashpassword);

    string_destroy(hashpassword);

    if (database_row_pop_integer(row) != 0) {
        web_session(web, client);
        http_send_file(client, "admin.html");
    } else {
        http_send_file(client, "invalid.html");
    }

    database_row_destroy(row);
}

/*
 * Will return true if the mutex is to be unlocked. Which is also
 * used to imply the termination of the web thread.
 */
static bool web_thread_quit(pthread_mutex_t *mutex) {
    switch (pthread_mutex_trylock(mutex)) {
        case 0:
            pthread_mutex_unlock(mutex);
            return 1;
        case EBUSY:
            return 0;
    }

    return 1;
}

static void *web_thread(void *data) {
    web_t *web = data;
    while (!web_thread_quit(&web->mutex))
        http_process(web->server);

    return NULL;
}

web_t *web_create(void) {
    http_t *server;
    if (!(server = http_create("5050")))
        return NULL;

    web_t *web    = malloc(sizeof(*web));
    web->rand     = mt_create();
    web->ripemd   = ripemd_create();
    web->database = database_create("web.db");
    web->server   = server;
    web->sessions = list_create();

    /* register intercepts for post POST */
    http_intercept_post(server, "::login", &web_login, web);

    /* register intercepts for GET */
    http_intercept_get(server, "",           "login.html", NULL,             web);
    http_intercept_get(server, "admin.html", "login.html", &web_check_admin, web);

    /*
     * The thread will keep running for as long as the mutex is locked.
     * The destruction is a simple unlock + join.  This is just a binary
     * semaphore. No need to wait.
     */
    pthread_mutex_init(&web->mutex, NULL);
    pthread_mutex_lock(&web->mutex);

    return web;
}

void web_destroy(web_t *web) {
    /* Unlock to terminate the thread and join. */
    pthread_mutex_unlock(&web->mutex);
    pthread_join(web->thread, NULL);

    mt_destroy(web->rand);
    ripemd_destroy(web->ripemd);
    database_destroy(web->database);
    http_destroy(web->server);

    /* destroy sessions */
    web_session_t *session;
    while ((session = list_pop(web->sessions)))
        web_session_destroy(session);
    list_destroy(web->sessions);

    free(web);
}

void web_begin(web_t *web) {
    pthread_create(&web->thread, NULL, &web_thread, (void*)web);
}
