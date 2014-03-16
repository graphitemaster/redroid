#include "irc.h"
#include "sock.h"
#include "module.h"
#include "command.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include <ctype.h>

int irc_join_raw(irc_t *irc, const char *ignore, const char *channel) {
    return 1;
}
int irc_quit_raw(irc_t *irc, const char *ignore, const char *message) {
    return 1;
}

int irc_quit(irc_t *irc, const char *message) {
    return 1;
}
int irc_join(irc_t *irc, const char *channel) {
    return 1;
}
int irc_part(irc_t *irc, const char *channel) {
    return 1;
}
int irc_action(irc_t *irc, const char *channel, const char *fmt, ...) {
    return 1;
}
int irc_write(irc_t *irc, const char *channel, const char *fmt, ...) {
    return 1;
}

const char *irc_nick(irc_t *irc) {
    return irc->nick;
}

const char *irc_name(irc_t *irc) {
    return irc->name;
}

bool irc_ready(irc_t *irc) {
    return irc->ready;
}

void irc_unqueue(irc_t *irc) {
}

static void irc_channels_join(irc_t *irc) {
    list_iterator_t *it = list_iterator_create(irc->channels);
    while (!list_iterator_end(it)) {
        irc_channel_t *channel = list_iterator_next(it);
        irc_join_raw(irc, NULL, channel->channel);
    }
    list_iterator_destroy(it);
}

static bool irc_channel_find_name(const void *a, const void *b) {
    const irc_channel_t *ca = a;
    const char          *cb = b;

    return !strcmp(ca->channel, cb);
}

static bool irc_channel_find(const void *a, const void *b) {
    return irc_channel_find_name(a, ((irc_channel_t*)b)->channel);
}

irc_t *irc_create(config_t *entry) {
    irc_t *irc = malloc(sizeof(irc_t));

    irc->name            = strdup(entry->name);
    irc->nick            = strdup(entry->nick);
    irc->pattern         = strdup(entry->pattern);
    irc->auth            = (entry->auth) ? strdup(entry->auth) : NULL;
    irc->ready           = false;
    irc->channels        = list_create();
    irc->database        = database_create(entry->database);
    irc->regexprcache    = regexpr_cache_create();
    irc->moduleman       = module_manager_create(irc);
    irc->message.nick    = NULL;
    irc->message.name    = NULL;
    irc->message.host    = NULL;
    irc->message.channel = NULL;
    irc->message.content = NULL;
    irc->buffer.data[0]  = '\0';
    irc->buffer.offset   = 0;

    printf("instance: %s\n", irc->name);
    printf("    nick     => %s\n", irc->nick);
    printf("    pattern  => %s\n", irc->pattern);
    printf("    auth     => %s\n", (irc->auth) ? irc->auth : "(None)");
    printf("    database => %s\n", entry->database);
    printf("    host     => %s\n", entry->host);
    printf("    port     => %s\n", entry->port);
    printf("    ssl      => %s\n", entry->ssl ? "Yes" : "No");

    return irc;
}

bool irc_modules_reload(irc_t *irc, const char *name) {
    return module_manager_module_reload(irc->moduleman, name);
}

bool irc_modules_add(irc_t *irc, const char *name) {
    string_t *error  = NULL;
    module_t *module = NULL;

    if (strstr(name, "//") || strstr(name, "./"))
        return false;

    string_t *file = string_format("modules/%s.so", name);
    if ((module = module_manager_module_search(irc->moduleman, string_contents(file), MMSEARCH_FILE))) {
        printf("    module   => %s [%s] already loaded\n", module->name, name);
        string_destroy(file);
        return false;
    }

    if ((module = module_open(string_contents(file), irc->moduleman, &error))) {
        printf("    module   => %s [%s] loaded\n", module->name, module->file);
        string_destroy(file);
        return true;
    }

    if (error) {
        printf("    module   => %s loading failed (%s)\n", name, string_contents(error));
        string_destroy(error);
    } else {
        printf("    module   => %s loading failed\n", name);
    }

    string_destroy(file);
    return false;
}

bool irc_modules_unload(irc_t *irc, const char *name) {
    return module_manager_module_unload(irc->moduleman, name);
}

static bool irc_modules_list_sort(const void *a, const void *b) {
    return strcmp(a, b) >= 0;
}

list_t *irc_modules_list(irc_t *irc) {
    list_t          *list = list_create();
    list_iterator_t *it   = list_iterator_create(irc->moduleman->modules);
    while (!list_iterator_end(it)) {
        module_t *entry = list_iterator_next(it);
        list_push(list, (void *)entry->name);
    }
    list_iterator_destroy(it);
    list_sort(list, &irc_modules_list_sort);
    return list;
}

list_t *irc_users(irc_t *irc, const char *chan) {
    irc_channel_t *channel = list_search(irc->channels, &irc_channel_find_name, chan);
    if (!channel)
        return NULL;

    list_t *copy = list_copy(channel->users);
    list_sort(copy, &irc_modules_list_sort);
    return copy;
}

bool irc_channels_add(irc_t *irc, const char *channel) {
    if (list_search(irc->channels, &irc_channel_find, channel)) {
        printf("    channel  => %s already exists\n", channel);
        return false;
    }

    irc_channel_t *ch = malloc(sizeof(*ch));
    ch->users   = list_create();
    ch->channel = strdup(channel);
    ch->topic   = NULL;

    list_push(irc->channels, ch);
    printf("    channel  => %s added\n", channel);
    return true;
}

void irc_destroy(irc_t *irc, sock_restart_t *restart, char **name) {
    if (irc->sock && !restart)
        irc_quit_raw(irc, NULL, "Shutting down");

    if (name)
        *name = strdup(irc->name);

    database_destroy(irc->database);
    regexpr_cache_destroy(irc->regexprcache);
    module_manager_destroy(irc->moduleman);

    list_iterator_t *it = list_iterator_create(irc->channels);
    while (!list_iterator_end(it)) {
        irc_channel_t   *ch = list_iterator_next(it);
        list_iterator_t *ut = list_iterator_create(ch->users);
        while (!list_iterator_end(ut))
            free(list_iterator_next(ut));
        list_iterator_destroy(ut);
        list_destroy(ch->users);
        free(ch->channel);
        free(ch->topic);
        free(ch);
    }
    list_iterator_destroy(it);
    list_destroy(irc->channels);

    free(irc->message.nick);
    free(irc->message.name);
    free(irc->message.host);
    free(irc->message.channel);
    free(irc->message.content);
    free(irc->auth);
    free(irc->nick);
    free(irc->name);
    free(irc->pattern);

    sock_destroy(irc->sock, restart);

    free(irc);
}

bool irc_connect(irc_t *irc, const char *host, const char *port, bool ssl) {
    sock_restart_t info = {
        .ssl = ssl,
        .fd  = -1
    };

    if (!(irc->sock = sock_create(host, port, &info)))
        return false;
    return true;
}

bool irc_reinstate(irc_t *irc, const char *host, const char *port, sock_restart_t *restart) {
    if (!(irc->sock = sock_create(host, port, restart)))
        return false;
    irc->ready = true;
    return true;
}

int irc_process(irc_t *irc, void *data) {
    if (!irc->sock)
        return 0;

    size_t get  = sock_recv(irc->sock, &irc->buffer.data[irc->buffer.offset], sizeof(irc->buffer.data) - irc->buffer.offset - 1);
    char  *find = strpbrk(irc->buffer.data, "\r\n");

    if (find) {
        /*
         * Some servers may send \r\n. For these servers we need to
         * null both termination points otherwise we'll double process
         * the line. Others only send \n.
         */
        char *more = strchr(find, '\n');
        if (more) find = more;
        find[0] = '\0';
        printf(">> %s\n", irc->buffer.data);

        /*
         * If the ending of the line exists before the end of the data
         * in the buffer then we need to move everything from the end
         * of the line to the beginning of the buffer.
         */
        if (find < &irc->buffer.data[irc->buffer.offset + get]) {
            size_t size = &irc->buffer.data[irc->buffer.offset + get] - find - 1;
            memmove(irc->buffer.data, find + 1, size);
            irc->buffer.offset = size;
        } else {
            /*
             * Otherwise we've processed the line and we can now clear
             * the rest of the buffer for more data.
             */
            irc->buffer.offset = 0;
            irc->buffer.data[0] = '\0';
        }
    } else {
        irc->buffer.offset += get;
        irc->buffer.data[irc->buffer.offset] = '\0';
    }

    return 1;
}
