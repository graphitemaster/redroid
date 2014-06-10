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
    list_t         *templates;
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

/* web string template engine */
typedef struct {
    hashtable_t *replaces;
    const char  *file;
    list_t      *lines;
    string_t    *formatted;
} web_template_t;

typedef struct {
    web_template_t *associated;
    char           *search;
    char           *replace;
} web_template_entry_t;

static void web_template_entries_destroy(web_template_entry_t *entry) {
    free(entry->search);
    free(entry->replace);
    free(entry);
}

static void web_template_entries_update(web_template_entry_t *entry) {
    string_t *find = string_format("<!--$[[%s]]-->", entry->search);
    string_replace(entry->associated->formatted, string_contents(find), entry->replace);
    string_destroy(find);
}

static void web_template_destroy(web_template_t *template) {
    string_destroy(template->formatted);
    hashtable_foreach(template->replaces, NULL, &web_template_entries_destroy);
    hashtable_destroy(template->replaces);

    char *line;
    while ((line = list_pop(template->lines)))
        free(line);
    list_destroy(template->lines);

    free(template);
}

static web_template_t *web_template_create(const char *infile) {
    size_t    size = 0;
    char     *line = NULL;
    string_t *file = string_format("site/%s", infile);
    FILE     *fp;

    if (!(fp = fopen(string_contents(file), "r")))
        return NULL;
    string_destroy(file);

    /* create new template and read in the file as individual lines */
    web_template_t tmpl = {
        .file      = infile,
        .replaces  = hashtable_create(256),
        .lines     = list_create(),
        .formatted = string_construct()
    };

    while (getline(&line, &size, fp) != EOF)
        list_push(tmpl.lines, strdup(line));

    free(line);
    fclose(fp);
    return memcpy(malloc(sizeof(tmpl)), &tmpl, sizeof(tmpl));
}

static void web_template_update(web_template_t *template) {
    (void)template;
#if 0
    string_destroy(template->formatted);
    template->formatted = string_construct();

    list_iterator_t *it = list_iterator_create(template->lines);
    while (!list_iterator_end(it)) {
        char *beg = list_iterator_next(it);
        char *end = beg;
        bool  def = false;

        /* deal with normal stuff */
        if (!(end = strstr(beg, "<!--$[[IF"))) {
            while (isspace(*beg)) beg++;
            string_catf(template->formatted, beg);
            continue;
        }

        beg = &end[strlen("<!--$[[IF")];
        if (!strncmp(beg, "DEF ", 4))
            def = true;
        else if (!strncmp(beg, "NDEF ", 5))
            def = false;
        else
            continue;

        if (!(end = strstr(beg, "]]-->")))
            continue;

        end[0] = '\0';
        web_template_entry_t *entry = hashtable_find(template->replaces, beg);
        char *ending = list_iterator_next(it);
        while (!list_iterator_end(it) && !strstr(ending, "<!--$[[ENDIF]]-->")) {
            if (def) {
                if (entry && entry->replace) {
                    while (isspace(*ending)) ending++;
                    string_catf(template->formatted, "%s", ending);
                }
            } else if (!entry || !entry->replace) {
                while (isspace(*ending)) ending++;
                string_catf(template->formatted, "%s", ending);
            }
            ending = list_iterator_next(it);
        }
        continue;
    }
    list_iterator_destroy(it);
    hashtable_foreach(template->replaces, NULL, &web_template_entries_update);
#endif
}

static web_template_t *web_template_find(web_t *web, const char *name) {
    return list_search(web->templates, name,
        lambda bool(const web_template_t *const template, const char *file) {
            return !strcmp(template->file, file);
        }
    );
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
        entry->search     = strdup(va_arg(va, char *));
        entry->replace    = NULL;
        entry->associated = find;

        hashtable_insert(find->replaces, entry->search, entry);
    }

    va_end(va);
}

static void web_template_change(web_t *web, const char *file, const char *search, const char *replace) {
    web_template_t *find = web_template_find(web, file);
    if (!find)
        return;

    web_template_entry_t *entry = hashtable_find(find->replaces, search);
    if (!entry)
        return;

    free(entry->replace);
    entry->replace = strdup(replace);
}

/* administration web template generation */
static void web_admin_create(web_t *web) {
    (void)web;
#if 0 /* TODO */
    string_t        *create = string_construct();
    list_t          *config = config_load("config.ini");
    list_iterator_t *it     = list_iterator_create(config);

    while (!list_iterator_end(it)) {
        config_t *entry = list_iterator_next(it);

        string_catf(create,
                "<h4>Instance: <span>%s</span> <i class='fa fa-arrow-circle-o-down pull-right'></i></h4>"
                "<div>"
                "<form method='post' action='/::update'>"
                "<div class='row'>"
                "<div class='col-xs-6'>"
                "<div class='instanceListHeader'>Bot data</div>"
                "<div class='form-group'>"
                "<input value='%s' type='hidden' name='instance'>"
                "<label for='nick'>Nickname</label>"
                "<input value='%s' type='text' class='form-control' name='nick' placeholder='Bot\'s nickname.'>"
                "</div>"
                "<div class='form-group'>"
                "<label for='pattern'>Bot pattern</label>"
                "<input value='%s' type='text' class='form-control' name='pattern' placeholder='The pattern you\'ll command the bot with.'>"
                "</div>"
                "<div class='form-group'>"
                "<label for='modules'>Modules (newline separated)</label>"
                "<textarea class='form-control' name='modules' placeholder='List of modules you want the bot to load upon start.' rows='4'>",
                entry->name,
                entry->name,
                entry->nick,
                entry->pattern);

        list_iterator_t *mo = list_iterator_create(entry->modules);
        while (!list_iterator_end(mo))
            string_catf(create, "%s\n", list_iterator_next(mo));
        list_iterator_destroy(mo);

        string_catf(create,
                "</textarea>"
                "</div>"
                "</div>"
                "<div class='col-xs-6'>"
                "<div class='instanceListHeader'>Server data</div>"
                "<div class='form-group'>"
                "<label for='host'>Server host</label>"
                "<input value='%s' type='text' class='form-control' name='host' placeholder='The server the bot will connect to.'>"
                "</div>"
                "<div class='form-group'>"
                "<label for='port'>Server port</label>"
                "<input value='%s' type='text' class='form-control' name='port' placeholder='The server\'s port.'>"
                "</div>"
                "<div class='form-group'>"
                "<label for='auth'>Authentication (NickServ)</label>"
                "<input value='%s' type='password' class='form-control' name='auth' placeholder='The bot\'s NickServ password (optional).'>"
                "</div>"
                "<div class='form-group'>"
                "<label for='channels'>Channels (newline separated)</label>"
                "<textarea class='form-control' name='channels' placeholder='List of channels you want the bot to join upon connect.' rows='4'>",
                entry->host,
                entry->port,
                (entry->auth) ? entry->auth : "");

        list_iterator_t *ct = list_iterator_create(entry->channels);
        while (!list_iterator_end(ct))
            string_catf(create, "%s\n", list_iterator_next(ct));
        list_iterator_destroy(ct);

        string_catf(create,
                "</textarea>"
                "</div>"
                "</div>"
                "<div class='col-xs-offset-10 col-xs-2'>"
                "<button type='submit' class='btn btn-block btn-danger'>Save <i class='fa fa-floppy-o'></i></button>"
                "</div>"
                "</div>"
                "</form>"
                "</div>"
                );

    }

    web_template_change(web, "admin.html", "INSTANCES", string_contents(create));

    list_iterator_destroy(it);
    config_unload(config);
    string_destroy(create);
#endif
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
        return web_template_send(web, client, "admin.html");
    }
    return web_template_send(web, client, "index.html");
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
        web_template_send(web, client, "admin.html");
    } else {
        web_template_change(web, "index.html", "ERROR", "Invalid username or password");
        web_template_send(web, client, "index.html");
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
        web_template_send(data, client, "index.html");
    } else if (!strcmp(control, "Settings")) {
        web_template_send(data, client, "system.html");
    } else {
        http_send_plain(client, "500 Internal Server Error");
    }
}

static void web_hook_postupdate(sock_t *client, list_t *post, void *data) {
    const char *name = http_post_find(post, "instance");
    if (!name)
        http_send_plain(client, "500 Internal Server Error");

#if 0
    /* TODO change config */
    const char *nick     = http_post_find(post, "nick");
    const char *pattern  = http_post_find(post, "pattern");
    const char *modules  = http_post_find(post, "modules");
    const char *host     = http_post_find(post, "host");
    const char *port     = http_post_find(post, "port");
    const char *auth     = http_post_find(post, "auth");
    const char *channels = http_post_find(post, "channels");
#endif

    web_template_change(data, "update.html", "INSTANCE", name);
    web_template_send(data, client, "update.html");
}

static void web_hook_postsystem(sock_t *client, list_t *post, void *data) {
    web_t      *web     = data;
    const char *control = http_post_find(post, "control");

    if (!control)
        return http_send_plain(client, "500 Internal Server Error");

    if (!strcmp(control, "Shutdown")) {
        web_template_change(data, "system.html", "ACTION", "Shutting down");
        web_template_send(data, client, "system.html");
        redroid_shutdown_global(web->ircmanager);
    } else if (!strcmp(control, "Restart")) {
        web_template_change(data, "system.html", "ACTION", "Restarting");
        web_template_send(data, client, "system.html");
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
    web->templates = list_create();

    http_intercept_post(server, "::login",  &web_hook_postlogin,  web);
    http_intercept_post(server, "::admin",  &web_hook_postadmin,  web);
    http_intercept_post(server, "::system", &web_hook_postsystem, web);
    http_intercept_post(server, "::update", &web_hook_postupdate, web);

    /* Register common landing page redirects */
    static const char *common_lands[] = { "admin", "index", "login", "home", "system", "update", "" };
    static const char *common_exts[]  = { "htm",   "html",  "HTM",   "HTML", "" };
    for (size_t i = 0; i < sizeof(common_lands)/sizeof(*common_lands); i++) {
        for (size_t j = 0; j < sizeof(common_exts)/sizeof(*common_exts); j++) {
            string_t *string = string_format("%s%s", common_lands[i], common_exts[j]);
            http_intercept_get(server, string_contents(string), &web_hook_redirect, web);
            string_destroy(string);
        }
    }

    web_template_register(web, "admin.html",  2, "INSTANCES", "VERSION");
    web_template_register(web, "index.html",  2, "ERROR",     "VERSION");
    web_template_register(web, "system.html", 2, "ACTION",    "VERSION");

    const char *build_info();
    web_template_change(web, "admin.html",  "VERSION", build_info());
    web_template_change(web, "index.html",  "VERSION", build_info());
    web_template_change(web, "system.html", "VERSION", build_info());

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
    list_foreach(web->templates, NULL, &web_template_destroy);
    list_destroy(web->templates);

    free(web);
}

void web_begin(web_t *web, irc_manager_t *manager) {
    web->ircmanager = manager; /* need manager to do shutdown */
    pthread_create(&web->thread, NULL, &web_thread, (void*)web);
}
