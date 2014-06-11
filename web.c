#include "web.h"
#include "http.h"
#include "mt.h"
#include "ripemd.h"
#include "database.h"
#include "string.h"
#include "config.h"
#include "hashtable.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <pthread.h>

void redroid_restart_global(irc_manager_t *irc);
void redroid_shutdown_global(irc_manager_t *irc);

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
    irc_manager_t  *ircmanager;
};

static web_session_t *web_session_create(sock_t *client) {
    web_session_t *session = malloc(sizeof(*session));
    session->host  = strdup(client->host);
    session->valid = true;
    return session;
}

static void web_session_destroy(web_session_t *session) {
    free(session->host);
    free(session);
}

static void web_session_control(web_t *web, sock_t *client, bool add) {
    /* Check if there isn't already a session for the client first */
    web_session_t *session = list_search(web->sessions, client,
        lambda bool(const web_session_t *const session, const sock_t *const sock) {
            return !strcmp(session->host, sock->host);
        }
    );
    if (add && session) {
        /* Revalidate session */
        session->valid = true;
        return;
    }

    if (add)
        list_push(web->sessions, web_session_create(client));
    else if (session)
        session->valid = false;
}

/* administration web template generation */
static void web_admin_create(web_t *web) {
    (void)web;
    /* TODO: Build admin page */
}

/* web hooks */
static void web_hook_redirect(sock_t *client, void *data) {
    web_t         *web     = data;
    web_session_t *session = list_search(web->sessions, client,
        lambda bool(const web_session_t *const session, const sock_t *const sock) {
            return !strcmp(session->host, sock->host);
        }
    );

    if (session && session->valid) {
        web_admin_create(web);
        /* TODO: Send template "admin.html" */
    }
    /* TODO: Send template "index.html" */
}

static void web_hook_postlogin(sock_t *client, list_t *post, void *data) {
    web_t *web = data;

    const char *username = http_post_find(post, "username");
    const char *password = http_post_find(post, "password");
    const char *remember = http_post_find(post, "remember_me");

    database_statement_t *statement = database_statement_create(web->database, "SELECT SALT FROM USERS WHERE USERNAME=?");
    database_row_t       *row       = NULL;

    if (!statement)
        return http_send_plain(client, "500 Internal Server Error");
    if (!database_statement_bind(statement, "s", username))
        return http_send_plain(client, "500 Internal Server Error");
    if (!(row = database_row_extract(statement, "s")))
        return http_send_plain(client, "500 Internal Server Error");

    const char    *getsalt         = database_row_pop_string(row);
    string_t      *saltpassword    = string_format("%s%s", getsalt, password);
    const char    *saltpasswordraw = string_contents(saltpassword);
    unsigned char *ripemdpass      = ripemd_compute(web->ripemd, saltpasswordraw, string_length(saltpassword));
    string_t      *hashpassword    = string_construct();

    string_destroy(saltpassword);
    database_row_destroy(row);
    free((void *)getsalt);

    if (!database_statement_complete(statement))
        return http_send_plain(client, "500 Internal Server Error");

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
        if (remember)
            web_session_control(web, client, true);
        web_admin_create(web);
        /* TODO: Send template "admin.html" */
    } else {
        /* TODO: Update "ERROR" "Invalid username or password" */
        /* TODO: Send template "index.html" */
    }

    database_statement_complete(statement);
    database_row_destroy(row);
}

static void web_hook_postadmin(sock_t *client, list_t *post, void *data) {
    const char *control = http_post_find(post, "control");
    if (!control)
        return http_send_plain(client, "500 Internal Server Error");

    if (!strcmp(control, "Logout")) {
        web_session_control(data, client, false);
        /* TODO: Send template "index.html" */
    } else if (!strcmp(control, "Settings")) {
        /* TODO: Send template "system.html" */
    } else {
        http_send_plain(client, "500 Internal Server Error");
    }
}

static void web_hook_postupdate(sock_t *client, list_t *post, void *data) {
    const char *name = http_post_find(post, "instance");
    if (!name)
        http_send_plain(client, "500 Internal Server Error");

    (void)data;

    /* TODO: Change configuration */

    /* TODO: Rebuild page */
}

static void web_hook_postsystem(sock_t *client, list_t *post, void *data) {
    web_t      *web     = data;
    const char *control = http_post_find(post, "control");

    if (!control)
        return http_send_plain(client, "500 Internal Server Error");

    (void)data;

    if (!strcmp(control, "Shutdown")) {
        /* TODO: build page ACTION "Shutdown" */
        redroid_shutdown_global(web->ircmanager);
    } else if (!strcmp(control, "Restart")) {
        /* TODO: build page ACTION "Restart" */
        redroid_restart_global(web->ircmanager);
    }
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

    web_t *web     = malloc(sizeof(*web));
    web->rand      = mt_create();
    web->ripemd    = ripemd_create();
    web->database  = database_create("web.db");
    web->server    = server;
    web->sessions  = list_create();

    http_intercept_post(server, "::login",  &web_hook_postlogin,  web);
    http_intercept_post(server, "::admin",  &web_hook_postadmin,  web);
    http_intercept_post(server, "::system", &web_hook_postsystem, web);
    http_intercept_post(server, "::update", &web_hook_postupdate, web);

    http_intercept_get(server, "index.html", &web_hook_redirect, web);
    http_intercept_get(server, "admin.html", &web_hook_redirect, web);

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
    /* Unlock will cause termination */
    pthread_mutex_unlock(&web->mutex);

    /*
     * The http server needs to be shutdown before we join the thread.
     * The reason for this is that the http_process may be in a blocking
     * state (stuck in poll). This means that we may not have actually
     * left the thread yet. http_destroy sends a wake event to a pipe
     * which will wake the poll and unblock. From here the thread is
     * finally left and we can join.
     */
    http_destroy(web->server);

    /* We can gracefully leave the thread now */
    pthread_join(web->thread, NULL);

    mt_destroy(web->rand);
    ripemd_destroy(web->ripemd);
    database_destroy(web->database);
    list_foreach(web->sessions, NULL, &web_session_destroy);
    list_destroy(web->sessions);

    free(web);
}

void web_begin(web_t *web, irc_manager_t *manager) {
    web->ircmanager = manager; /* need manager to do shutdown */
    pthread_create(&web->thread, NULL, &web_thread, (void*)web);
}
