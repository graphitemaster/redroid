#include "sock.h"
#include "list.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* For testing */
#ifndef HTTP_MAIN
#   define HTTP_MAIN http_main
#endif

#define ADMIN_USER "graphitemaster"
#define ADMIN_PASS "password"

typedef struct {
    char *host;
    char *username;
    char *password;
    bool  remember;
    bool  invalid;
    bool  expired;
} html_session_t;

/* Session manegement */
static list_t *http_sessions = NULL;

static bool html_session_search(const void *a, const void *b) {
    html_session_t *sa = (html_session_t*)a;
    return !strcmp(sa->host, (const char *)b);
}

static void http_send_header(sock_t *client, const char *type) {
    sock_sendf(client, "HTTP/1.0 200 OK\r\n");
    sock_sendf(client, "Content-Type: %s\r\n", type);
    sock_sendf(client, "Server: Redroid HTTP\r\n\r\n");
}

static void http_send_redirect(sock_t *client, const char *location) {
    sock_sendf(client, "HTTP/1.0 301 Moved Permanently\r\n");
    sock_sendf(client, "Location: %s\r\n", location);
    sock_sendf(client, "Server: Redroid HTTP\r\n\r\n");
}

static void html_send_common_header(sock_t *client, const char *title) {
    sock_sendf(client, "<!DOCTYPE html>\n");
    sock_sendf(client, "    <head>\n");
    sock_sendf(client, "        <meta charset=\"utf-8\">\n");
    sock_sendf(client, "        <meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge,chrome=1\">\n");
    sock_sendf(client, "        <title>Redroid - %s</title>\n", title);
    sock_sendf(client, "        <link rel=stylesheet href=\"style.css\" type=\"text/css\" media=screen>\n");
    sock_sendf(client, "        <meta name=\"robots\" content=\"noindex,follow\" />\n");
    sock_sendf(client, "        <script language=\"javascript\" type=\"text/javascript\">\n");
    sock_sendf(client, "            function toggle(identifier) {\n");
    sock_sendf(client, "                divide = document.getElementById(identifier)\n");
    sock_sendf(client, "                expand = document.getElementById(\"x\" + identifier)\n");
    sock_sendf(client, "                if (divide.style.display == \"none\") {\n");
    sock_sendf(client, "                    divide.style.display = \"block\"\n");
    sock_sendf(client, "                    expand.innerHTML = \"[-]\"\n");
    sock_sendf(client, "                } else {\n");
    sock_sendf(client, "                    divide.style.display = \"none\"\n");
    sock_sendf(client, "                    expand.innerHTML = \"[+]\"\n");
    sock_sendf(client, "                }\n");
    sock_sendf(client, "            }\n");
    sock_sendf(client, "        </script>\n");
    sock_sendf(client, "    </head>\n");
    sock_sendf(client, "    <body>\n");
    sock_sendf(client, "        <div class=\"container\">\n");
}

static void html_send_common_footer(sock_t *client) {
    sock_sendf(client, "        </div>\n");
    sock_sendf(client, "    </body>\n");
    sock_sendf(client, "</html>\n");
}

static void html_send_login(sock_t *client, bool failed) {
    char *username = NULL;
    char *password = NULL;

    http_send_header(client, "text/html");
    html_send_common_header(client, "Login");

    /* Check the session for an existing log in */
    html_session_t *find = list_search(http_sessions, &html_session_search, client->host);
    if (find && find->remember) {
        username = find->username;
        password = find->password;
    }

    if (!username || !password) {
        username = "";
        password = "";
    }

    sock_sendf(client, "            <div class=\"login\">\n");
    sock_sendf(client, "                <h1>Login to Redroid</h1>\n");

    if (failed)
        sock_sendf(client, "                <h3>Invalid user name or password</h3>\n");

    sock_sendf(client, "                <form method=\"post\" action=\"/::login\" autocomplete=\"off\">\n");
    sock_sendf(client, "                    <p><input type=\"text\" name=\"login\" value=\"%s\" placeholder=\"Username\"></p>\n", username);
    sock_sendf(client, "                    <p><input type=\"password\" name=\"password\" value=\"%s\" placeholder=\"Password\"></p>\n", password);
    sock_sendf(client, "                    <p class=\"remember_me\">\n");
    sock_sendf(client, "                        <label>\n");
    sock_sendf(client, "                            <input type=\"checkbox\" name=\"remember_me\" id=\"remember_me\">\n");
    sock_sendf(client, "                            Remember me on this computer\n");
    sock_sendf(client, "                        </label>\n");
    sock_sendf(client, "                    </p>\n");
    sock_sendf(client, "                    <p class=\"submit\"><input type=\"submit\" name=\"commit\" value=\"Login\"></p>\n");
    sock_sendf(client, "                </form>\n");
    sock_sendf(client, "            </div>\n");
    sock_sendf(client, "            <div class=\"login-help\">\n");
    sock_sendf(client, "                <p>Forgot your password? <a href=\"#\">Click here to reset it</a>.</p>\n");
    sock_sendf(client, "            </div>\n");

    html_send_common_footer(client);
}

static void html_send_admin(sock_t *client) {
    http_send_header(client, "text/html");
    html_send_common_header(client, "Administration");
    sock_sendf(client, "            <div class=\"admin\">\n");
    sock_sendf(client, "                <h1>Administrate</h1>\n");

    list_t          *config = config_load("config.ini");
    list_iterator_t *it     = list_iterator_create(config);

    while (!list_iterator_end(it)) {
        config_t *entry = list_iterator_next(it);

        sock_sendf(
            client,
            "<a href=\"javascript:toggle('%s');\" id=\"x%s\">[+]</a><strong>%s</strong>\n",
            entry->name,
            entry->name,
            entry->name
        );

        sock_sendf(
            client,
            "<div id=\"%s\" style=\"display: none;\">\n",
            entry->name
        );


        sock_sendf(client, "<form method=\"post\" action=\"/::update\">");
        sock_sendf(client, "<p class=\"left\">Nickname</p>\n");
        sock_sendf(client, "<p class=\"right\"><input type=\"text\" name=\"nick\" value=\"%s\"></p>\n", entry->nick);
        sock_sendf(client, "<p class=\"left\">Server host</p>\n");
        sock_sendf(client, "<p class=\"right\"><input type=\"text\" name=\"host\" value=\"%s\"></p>\n", entry->host);
        sock_sendf(client, "<p class=\"left\">Server port</p>\n");
        sock_sendf(client, "<p class=\"right\"><input type=\"text\" name=\"port\" value=\"%s\"></p>\n", entry->port);
        sock_sendf(client, "<p class=\"left\">Authentication (NickServ)</p>\n");
        sock_sendf(client, "<p class=\"right\"><input type=\"password\" name=\"auth\" value=\"%s\"></p>\n", (entry->auth) ? entry->auth : "");
        sock_sendf(client, "<p class=\"left\">Bot pattern</p>\n");
        sock_sendf(client, "<p class=\"right\"><input type=\"text\" name=\"pattern\" value=\"%s\"></p>\n", entry->pattern);
        sock_sendf(client, "<p class=\"left\">Channels (newline seperated)</p>\n");
        sock_sendf(client, "<p class=\"right\">\n");
        sock_sendf(client, "    <textarea cols=\"10\" rows=\"10\" name=\"channels\">\n");

        list_iterator_t *ct = list_iterator_create(entry->channels);
        while (!list_iterator_end(ct))
            sock_sendf(client, "%s\n", list_iterator_next(ct));
        list_iterator_destroy(ct);
        sock_sendf(client, "</textarea>\n");

        sock_sendf(client, "</p>\n");
        sock_sendf(client, "<p class=\"left\">Modules (newline seperated)</p>\n");
        sock_sendf(client, "<p class=\"right\">\n");
        sock_sendf(client, "    <textarea cols=\"10\" rows=\"10\" name=\"modules\">\n");

        list_iterator_t *mo = list_iterator_create(entry->modules);
        while (!list_iterator_end(mo))
            sock_sendf(client, "%s\n", list_iterator_next(mo));
        list_iterator_destroy(mo);
        sock_sendf(client, "</textarea>\n");

        sock_sendf(client, "</p>\n");
        sock_sendf(client, "<p class=\"submit\">\n");
        sock_sendf(client, "    <p class=\"left\"></p>\n");
        sock_sendf(client, "    <input type=\"submit\" name=\"commit\" value=\"Save\">\n");
        sock_sendf(client, "</p>\n");
        sock_sendf(client, "</div><br />\n");
    }

    config_unload(config);
    list_iterator_destroy(it);

    sock_sendf(client, "            </div>\n");
    html_send_common_footer(client);
}

static void html_send_404(sock_t *client) {
    http_send_header(client, "text/html");
    sock_sendf(client, "<html>\n");
    sock_sendf(client, "<h1>404 File not found</h1>\n");
    sock_sendf(client, "</html>\n");
}

static void html_send_file(sock_t *client, const char *file) {
    char *dot = strrchr(file, '.');
    /* The front end only depends on CSS for now */
    if (!strcmp(dot, ".css"))
        http_send_header(client, "text/css");
    else
        http_send_header(client, "text/plain");

    /* Open the file and send it out over the socket line by line */
    FILE *fp;
    if (!(fp = fopen(file, "r")))
        html_send_404(client);

    char  *line = NULL;
    size_t size = 0;

    while (getline(&line, &size, fp) != EOF)
        sock_sendf(client, line);

    free(line);
    fclose(fp);
}

static void html_send_common(sock_t *client, const char *common) {
    /* If there is an extension then we send a file */
    if (strchr(common, '.'))
        return html_send_file(client, common);

    /* If we're not in session, or it's an invalid one then try logging in again */
    html_session_t *session = list_search(http_sessions, &html_session_search, client->host);
    if (!session || session->expired)
        return html_send_login(client, false);
    else if (session->invalid) {
        list_erase(http_sessions, session);
        return html_send_login(client, true);
    }

    /* Sessions expire after one use */
    session->expired = true;
    return html_send_admin(client);
}

/* Key value store of posted data */
typedef struct {
    char *key;
    char *value;
} html_post_content_t;

static html_post_content_t *html_post_parse_keyvalue(char *content) {
    html_post_content_t *keyvalue = malloc(sizeof(*keyvalue));
    char                *cursor   = strchr(content, '=');

    *cursor = '\0';
    keyvalue->key   = strdup(content);
    keyvalue->value = strdup(&cursor[1]);

    return keyvalue;
}

static list_t *html_post_parse(char *content) {
    list_t *list = list_create();
    char   *next = strtok(content, "&");

    while (next) {
        list_push(list, html_post_parse_keyvalue(next));
        next = strtok(NULL, "&");
    }

    return list;
}

static void html_post_parse_cleanup(list_t *list) {
    list_iterator_t *it = list_iterator_create(list);
    while (!list_iterator_end(it)) {
        html_post_content_t *content = list_iterator_next(it);
        free(content->key);
        free(content->value);
        free(content);
    }
    list_iterator_destroy(it);
    list_destroy(list);
}

static bool html_post_search(const void *a, const void *b) {
    html_post_content_t *ca = (html_post_content_t*)a;
    return !strcmp(ca->key, (const char *)b);
}

static void html_post_login(sock_t *client, char *content) {
    list_t *postdata = html_post_parse(content);

    /* Post login requires two pieces of data. Username and password */
    html_post_content_t *username = list_search(postdata, &html_post_search, "login");
    html_post_content_t *password = list_search(postdata, &html_post_search, "password");
    html_post_content_t *remember = list_search(postdata, &html_post_search, "remember_me");

    /* Check for existing session */
    html_session_t *find = list_search(http_sessions, &html_session_search, client->host);
    if (find) {
        list_erase(http_sessions, find);

        free(find->host);
        free(find->username);
        free(find->password);
        free(find);
    }

    /* Create the session */
    html_session_t *session = malloc(sizeof(*session));
    session->host     = strdup(client->host);
    session->username = strdup(username->value);
    session->password = strdup(password->value);
    session->remember = !!remember;
    session->expired  = false;

    /* TODO: check database and hash passwords */
    if (!strcmp(session->username, ADMIN_USER) &&
        !strcmp(session->password, ADMIN_PASS))
        session->invalid = false;
    else
        session->invalid = true;


    list_push(http_sessions, session);
    html_post_parse_cleanup(postdata);
    http_send_redirect(client, "/");
}

int HTTP_MAIN(void) {
    http_sessions = list_create();
    sock_t *socket = sock_create("::listen", "5050", SOCK_RESTART_NIL);

    if (!socket) {
        fprintf(stderr, "failed creating connection\n");
        sock_destroy(socket, NULL);
        return -1;
    }

    for (;;) {
        sock_t *client = sock_accept(socket);
        if (!client)
            continue; /* no new clients */

        char request[4096];
        memset(request, 0, sizeof(request));
        sock_recv(client, request, sizeof(request));

        if (!strncmp(request, "GET /", 5)) {
            char *file  = &request[5];
            char *strip = strstr(file, " HTTP");

            if (!strip) {
                sock_destroy(client, NULL);
                continue;
            }
            *strip = '\0';
            html_send_common(client, file);
        } else if (!strncmp(request, "POST /", 6)) {
            char *file    = &request[6];
            char *content = strstr(file, "\r\n\r\n");

            content += 4;
            printf("%s\n", content);
            if (!strncmp(file, "::login", 7))
                html_post_login(client, content);
            /* TODO: deal with update */
        }
        sock_destroy(client, NULL);
    }
    sock_destroy(socket, NULL);
    return 0;
}
