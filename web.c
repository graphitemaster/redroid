#include "web.h"
#include "http.h"
#include "mt.h"
#include "ripemd.h"
#include "database.h"
#include "string.h"
#include "config.h"

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
    list_t         *templates;
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

static void web_session_control(web_t *web, sock_t *client, bool add) {
    /* Check if there isn't already a session for the client first */
    web_session_t *session = list_search(web->sessions, &web_session_search, client);
    if (add && session) {
        /* Revalidate session */
        session->valid = true;
        return;
    }

    if (add)
        list_push(web->sessions, web_session_create(web, client));
    else if (session)
        session->valid = false;
}

/* web string template engine */
typedef struct {
    list_t     *replaces;
    const char *file;
    char       *raw;
    string_t   *formatted;
} web_template_t;

typedef struct {
    char *search;
    char *replace;
} web_template_entry_t;

static void web_template_destroy(web_template_t *template) {
    free(template->raw);
    string_destroy(template->formatted);

    list_iterator_t *it = list_iterator_create(template->replaces);
    while (!list_iterator_end(it)) {
        web_template_entry_t *entry = list_iterator_next(it);
        free(entry->search);
        free(entry->replace);
        free(entry);
    }
    list_iterator_destroy(it);

    list_destroy(template->replaces);
    free(template);
}

static web_template_t *web_template_create(const char *infile) {
    string_t *file = string_format("site/%s", infile);
    FILE     *fp;

    if (!(fp = fopen(string_contents(file), "r"))) {
        printf("file not found!\n");
        return NULL;
    }

    string_destroy(file);

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    web_template_t *tmpl = malloc(sizeof(*tmpl));

    tmpl->raw      = malloc(size + 1);
    tmpl->file     = infile;
    tmpl->replaces = list_create();

    if (fread(tmpl->raw, size, 1, fp) != 1)
        goto web_template_error;

    tmpl->raw[size] = '\0';
    tmpl->formatted = string_construct();

    fclose(fp);
    return tmpl;

web_template_error:
    fclose(fp);
    web_template_destroy(tmpl);
    return NULL;
}

static void web_template_update(web_template_t *template) {
    string_destroy(template->formatted);
    template->formatted = string_create(template->raw);

    list_iterator_t *it = list_iterator_create(template->replaces);
    while (!list_iterator_end(it)) {
        web_template_entry_t *entry = list_iterator_next(it);
        if (!entry->replace)
            continue;
        string_t *format = string_format("<!--$[[%s]]-->", entry->search);
        string_replace(template->formatted, string_contents(format), entry->replace);
        string_destroy(format);
    }
    list_iterator_destroy(it);
}

static bool web_template_search(const void *a, const void *b) {
    const web_template_t *const wa = a;
    const char           *const aa = wa->file;
    const char           *const bb = (const char *const)b;

    return !strcmp(aa, bb);
}

static bool web_template_entry_search(const void *a, const void *b) {
    const web_template_entry_t *ea = a;
    return !strcmp(ea->search, (const char *)b);
}

static web_template_t *web_template_find(web_t *web, const char *name) {
    return list_search(web->templates, &web_template_search, name);
}

static web_template_entry_t *web_template_entry_find(web_template_t *template, const char *search) {
    return list_search(template->replaces, &web_template_entry_search, search);
}

static void web_template_send(web_t *web, sock_t *client, const char *tmpl) {
    web_template_t *find = web_template_find(web, tmpl);
    if (!find)
        return http_send_plain(client, "404 File not found");

    web_template_update(find);
    http_send_html(client, string_contents(find->formatted));
}

static void web_template_register(web_t *web, const char *file, size_t count, ...) {
    web_template_t *find = web_template_find(web, file);
    if (!find) {
        find = web_template_create(file);
        list_push(web->templates, find);
    }

    va_list  va;
    va_start(va, count);

    for (size_t i = 0; i < count; i++) {
        web_template_entry_t *entry = malloc(sizeof(*entry));
        entry->search  = strdup(va_arg(va, char *));
        entry->replace = NULL;
        list_push(find->replaces, entry);
    }

    va_end(va);
}

static void web_template_change(web_t *web, const char *file, const char *search, const char *replace) {
    web_template_t *find = web_template_find(web, file);
    if (!find)
        return;

    web_template_entry_t *entry = web_template_entry_find(find, search);
    if (!entry)
        return;

    free(entry->replace);
    entry->replace = strdup(replace);
}

/* administration web template generation */
static void web_admin_create(web_t *web) {
    string_t        *create = string_construct();
    list_t          *config = config_load("config.ini");
    list_iterator_t *it     = list_iterator_create(config);

    while (!list_iterator_end(it)) {
        config_t *entry = list_iterator_next(it);
        string_catf(
            create,
            "<a href=\"javascript:toggle('%s');\" id=\"x%s\">[+]</a><strong>%s</strong>\n",
            entry->name,
            entry->name,
            entry->name
        );

        string_catf(
            create,
            "               <div id=\"%s\" style=\"display: none;\">\n",
            entry->name
        );


        string_catf(create, "<form method=\"post\" action=\"/::update\">");
        string_catf(create, "<p class=\"left\">Nickname</p>\n");
        string_catf(create, "<p class=\"right\"><input type=\"text\" name=\"nick\" value=\"%s\"></p>\n", entry->nick);
        string_catf(create, "<p class=\"left\">Server host</p>\n");
        string_catf(create, "<p class=\"right\"><input type=\"text\" name=\"host\" value=\"%s\"></p>\n", entry->host);
        string_catf(create, "<p class=\"left\">Server port</p>\n");
        string_catf(create, "<p class=\"right\"><input type=\"text\" name=\"port\" value=\"%s\"></p>\n", entry->port);
        string_catf(create, "<p class=\"left\">Authentication (NickServ)</p>\n");
        string_catf(create, "<p class=\"right\"><input type=\"password\" name=\"auth\" value=\"%s\"></p>\n", (entry->auth) ? entry->auth : "");
        string_catf(create, "<p class=\"left\">Bot pattern</p>\n");
        string_catf(create, "<p class=\"right\"><input type=\"text\" name=\"pattern\" value=\"%s\"></p>\n", entry->pattern);
        string_catf(create, "<p class=\"left\">Channels (newline seperated)</p>\n");
        string_catf(create, "<p class=\"right\">\n");
        string_catf(create, " <textarea cols=\"10\" rows=\"10\" name=\"channels\">\n");

        list_iterator_t *ct = list_iterator_create(entry->channels);
        while (!list_iterator_end(ct))
            string_catf(create, "%s\n", list_iterator_next(ct));
        list_iterator_destroy(ct);
        string_catf(create, "</textarea>\n");

        string_catf(create, "</p>\n");
        string_catf(create, "<p class=\"left\">Modules (newline seperated)</p>\n");
        string_catf(create, "<p class=\"right\">\n");
        string_catf(create, " <textarea cols=\"10\" rows=\"10\" name=\"modules\">\n");

        list_iterator_t *mo = list_iterator_create(entry->modules);
        while (!list_iterator_end(mo))
            string_catf(create, "%s\n", list_iterator_next(mo));
        list_iterator_destroy(mo);
        string_catf(create, "</textarea>\n");

        string_catf(create, "</p>\n");
        string_catf(create, "<p class=\"submit\">\n");
        string_catf(create, " <p class=\"left\"></p>\n");
        string_catf(create, " <input type=\"submit\" name=\"commit\" value=\"Save\">\n");
        string_catf(create, "</p>\n");
        string_catf(create, "</div><br />\n");

    }

    web_template_change(web, "admin.html", "INSTANCES", string_contents(create));

    list_iterator_destroy(it);
    config_unload(config);
    string_destroy(create);
}

/* web hooks */
static void web_hook_redirect(sock_t *client, void *data) {
    web_t         *web     = data;
    web_session_t *session = list_search(web->sessions, &web_session_search, client);

    if (session && session->valid) {
        web_admin_create(web);
        return web_template_send(web, client, "admin.html");
    }
    return http_send_file(client, "login.html");
}

static void web_hook_postlogin(sock_t *client, list_t *post, void *data) {
    web_t *web = data;

    const char *username = http_post_find(post, "username");
    const char *password = http_post_find(post, "password");
    const char *remember = http_post_find(post, "remember_me");

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
    free((void *)getsalt);

    if (!database_statement_complete(statement))
        return http_send_file(client, "invalid.html");

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
        web_template_send(web, client, "admin.html");
    } else {
        web_template_change(web, "login.html", "ERROR", "<h2>Invalid username or password</h2>");
        web_template_send(web, client, "login.html");
    }

    database_statement_complete(statement);
    database_row_destroy(row);
}

static void web_hook_postadmin(sock_t *client, list_t *post, void *data) {
    const char *control = http_post_find(post, "control");
    if (!control)
        http_send_plain(client, "301 Operation not supported");

    if (!strcmp(control, "Logout")) {
        web_session_control(data, client, false);
        http_send_file(client, "login.html");
    } else if (!strcmp(control, "Settings")) {
        http_send_plain(client, "TODO: settings");
    } else {
        http_send_unimplemented(client);
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
    web->templates = list_create();

    /* register intercepts for post POST */
    http_intercept_post(server, "::login", &web_hook_postlogin, web);
    http_intercept_post(server, "::admin", &web_hook_postadmin, web);

    /* Register common landing page redirects */
    static const char *common_lands[] = { "admin", "index", "login", "home", "" };
    static const char *common_exts[]  = { "htm",   "html",  "HTM",   "HTML", "" };
    for (size_t i = 0; i < sizeof(common_lands)/sizeof(*common_lands); i++) {
        for (size_t j = 0; j < sizeof(common_exts)/sizeof(*common_exts); j++) {
            string_t *string = string_format("%s%s", common_lands[i], common_exts[j]);
            http_intercept_get(server, string_contents(string), &web_hook_redirect, web);
            string_destroy(string);
        }
    }

    /* register some templates */
    web_template_register(web, "admin.html", 1, "INSTANCES");
    web_template_register(web, "login.html", 1, "ERROR");

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

    /* destroy templates */
    web_template_t *template;
    while ((template = list_pop(web->templates)))
        web_template_destroy(template);
    list_destroy(web->templates);

    free(web);
}

void web_begin(web_t *web) {
    pthread_create(&web->thread, NULL, &web_thread, (void*)web);
}
