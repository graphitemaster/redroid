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

/*
 * Queue for IRC messages and the API to us esaid queue from modules
 * and other things. The RAW api isn't buffered so use at your own
 * peril (typically for things where you're not too worried about flood
 * and need it out as soon as possible).
 */
static int irc_action_raw(irc_t *irc, const char *channel, const char *message);
static int irc_quit_raw(irc_t *irc, const char *channel, const char *message);
static int irc_join_raw(irc_t *irc, const char *channel, const char *message);

typedef struct {
    char     *channel;
    string_t *message;
    int  (*raw)(irc_t *irc, const char *channel, const char *message);
} irc_queue_entry_t;

void irc_queue_enqueue(irc_t *irc, int (*raw)(irc_t *irc, const char *, const char *), const char *channel, string_t *message) {
    list_push(
        irc->queue,
        memcpy(
            malloc(sizeof(irc_queue_entry_t)),
            &(irc_queue_entry_t) {
                .channel = (channel) ? strdup(channel) : NULL, // may not come from a channel (e.g /quit /join)
                .message = message,
                .raw     = raw
            },
            sizeof(irc_queue_entry_t)
        )
    );
}

void irc_queue_entry_destroy(irc_queue_entry_t *entry) {
    // not all commands will come from a channel
    if (entry->channel)
        free(entry->channel);

    string_destroy(entry->message);
    free(entry);
}

bool irc_queue_dequeue(irc_t *irc) {
    irc_queue_entry_t *entry = list_shift(irc->queue);
    if (!entry)
        return false;

    entry->raw(irc, entry->channel, string_contents(entry->message));
    irc_queue_entry_destroy(entry);

    return true;
}

static void irc_queue_destroy(irc_t *irc) {
    while (irc_queue_dequeue(irc))
        ;

    list_destroy(irc->queue);
}

static int irc_pong(irc_t *irc, const char *data) {
    return sock_sendf(irc->sock, "PONG :%s\r\n", data);
}
static int irc_register(irc_t *irc) {
    return sock_sendf(irc->sock, "NICK %s\r\nUSER %s localhost 0 :redroid\r\n", irc->nick, irc->nick);
}
static int irc_quit_raw(irc_t *irc, const char *channel, const char *message) {
    return sock_sendf(irc->sock, "QUIT :%s\r\n", message);
}
static int irc_join_raw(irc_t *irc, const char *channel, const char *message) {
    return sock_sendf(irc->sock, "JOIN %s\r\n", message);
}
static int irc_part_raw(irc_t *irc, const char *channel, const char *message) {
    return sock_sendf(irc->sock, "PART %s\r\n", message);
}
static int irc_write_raw(irc_t *irc, const char *channel, const char *data) {
    return sock_sendf(irc->sock, "PRIVMSG %s :%s\r\n", channel, data);
}
static int irc_action_raw(irc_t *irc, const char *channel, const char *data) {
    return sock_sendf(irc->sock, "PRIVMSG %s :\001ACTION %s\001\r\n", channel, data);
}

// DO NOT USE these from irc_process or irc_process_line
// these are for modules which run in a seperate thread.
int irc_quit(irc_t *irc, const char *message) {
    irc_queue_enqueue(irc, &irc_quit_raw, NULL, string_create(message)); // freed in irc_queue_dequeue
    return 1;
}
int irc_join(irc_t *irc, const char *channel) {
    irc_queue_enqueue(irc, &irc_join_raw, NULL, string_create(channel)); // freed in irc_queue_dequeue
    return 1;
}
int irc_part(irc_t *irc, const char *channel) {
    irc_queue_enqueue(irc, &irc_part_raw, NULL, string_create(channel));
    return 1;
}
int irc_action(irc_t *irc, const char *channel, const char *fmt, ...) {
    string_t *string = string_construct();
    va_list va;
    va_start(va, fmt);
    string_vcatf(string, fmt, va);
    va_end(va);
    irc_queue_enqueue(irc, &irc_action_raw, channel, string);
    return 1;
}
int irc_write(irc_t *irc, const char *channel, const char *fmt, ...) {
    string_t *string = string_construct();
    va_list  va;
    va_start(va, fmt);
    string_vcatf(string, fmt, va);
    va_end(va);
    irc_queue_enqueue(irc, &irc_write_raw, channel, string);
    return 1;
}

const char *irc_nick(irc_t *irc) {
    return irc->nick;
}

const char *irc_name(irc_t *irc) {
    return irc->name;
}

static void irc_onnick(irc_t *irc, irc_parser_data_t *data) {
    if (!strncmp(data->content, "NickServ", data->length) && !(irc->flags & IRC_STATE_NICKSERV)) {
        irc_write(irc, "NickServ", "IDENTIFY %s %s", irc->nick, irc->auth);
        irc->flags |= IRC_STATE_NICKSERV;
    }

    free(irc->message.nick);
    irc->message.nick = strdup(data->content);
}

static void irc_onname(irc_t *irc, irc_parser_data_t *data) {
    free(irc->message.name);
    irc->message.name = strdup(data->content);
}

static void irc_onhost(irc_t *irc, irc_parser_data_t *data) {
    free(irc->message.host);
    irc->message.host = strdup(data->content);
}

static void irc_onparam(irc_t *irc, irc_parser_data_t *data) {
    if (!strncmp(data->content, "AUTH", data->length))
        irc->flags |= IRC_STATE_AUTH;
    if (!strncmp(data->content, irc->nick, data->length))
        irc->flags |= IRC_COMMAND_KICK;

    /*
     * RFC (2812): 1.3 Channels
     *  Channel names are strings (beginning with a '&', '#', '+' or '!'
     *  character).
     */
    if (strpbrk(data->content, "&#+!"))
        irc->message.channel = strdup(data->content);
}

static void irc_onerror(irc_t *irc, irc_parser_data_t *data) {
    /* broadcast all errors and shut down */
    list_iterator_t *it = list_iterator_create(irc->channels);
    while (!list_iterator_end(it))
        irc_write(irc, list_iterator_next(it), "error %s (shutting down)", data->content);
    list_iterator_destroy(it);
    raise(SIGUSR1);
}

static void irc_channels_join(irc_t *irc) {
    list_iterator_t *it = list_iterator_create(irc->channels);
    while (!list_iterator_end(it))
        irc_join_raw(irc, NULL, list_iterator_next(it));
    list_iterator_destroy(it);
}

static void irc_oncommand(irc_t *irc, irc_parser_data_t *data) {
    static const struct {
        const char *name;
        size_t      flag;
    } commands[] = {
        { "PING",  IRC_COMMAND_PING  },
        { "ERROR", IRC_COMMAND_ERROR },
        { "KICK",  IRC_COMMAND_KICK  },
        { "JOIN",  IRC_COMMAND_JOIN  },
        { "LEAVE", IRC_COMMAND_LEAVE }
    };
    for (size_t i = 0; i < sizeof(commands)/sizeof(*commands); i++)
        if (!strcmp(data->content, commands[i].name))
            irc->flags |= commands[i].flag;
}

static void irc_onend(irc_t *irc, irc_parser_data_t *data) {
    if (irc->flags & IRC_COMMAND_ERROR) {
        fprintf(stderr, "    irc      => %s\n", data->content);
        irc->flags &= ~IRC_COMMAND_ERROR;
    }

    if (!(irc->flags & IRC_STATE_AUTH)) {
        irc_register(irc);
        irc->flags |= IRC_STATE_AUTH;
    }

    if (irc->flags & IRC_COMMAND_PING) {
        irc_pong(irc, data->content);
        irc->flags &= ~IRC_COMMAND_PING;
    }

    if (irc->flags & IRC_COMMAND_KICK) {
        irc_channels_join(irc);
        irc->flags |= IRC_STATE_READY;
        irc->flags &= ~IRC_COMMAND_KICK;
    }

    free(irc->message.content);
    irc->message.content = strdup(data->content);
    irc->flags |= IRC_STATE_END;
}


// Instance management
irc_t *irc_create(config_t *entry) {
    irc_t *irc = malloc(sizeof(irc_t));

    if (!(irc->name    = strdup(entry->name)))    goto error;
    if (!(irc->nick    = strdup(entry->nick)))    goto error;
    if (!(irc->pattern = strdup(entry->pattern))) goto error;

    if (entry->auth) {
        if (!(irc->auth = strdup(entry->auth)))
            goto error;
    } else {
        irc->auth = NULL;
    }

    irc->channels     = list_create();
    irc->queue        = list_create();
    irc->database     = database_create(entry->database);
    irc->regexprcache = regexpr_cache_create();
    irc->moduleman    = module_manager_create(irc);
    irc->flags        = 0;

    irc->message.nick    = NULL;
    irc->message.name    = NULL;
    irc->message.host    = NULL;
    irc->message.channel = NULL;
    irc->message.content = NULL;

    printf("instance: %s\n", irc->name);
    printf("    nick     => %s\n", irc->nick);
    printf("    pattern  => %s\n", irc->pattern);
    printf("    auth     => %s\n", (irc->auth) ? irc->auth : "(None)");
    printf("    database => %s\n", entry->database);
    printf("    host     => %s\n", entry->host);
    printf("    port     => %s\n", entry->port);
    printf("    ssl      => %s\n", entry->ssl ? "Yes" : "No");

    irc_parser_init(&irc->parser, irc);

    /* Register the callbacks */
    irc->parser.callbacks[IRC_PARSER_STATE_NICK]      = &irc_onnick;
    irc->parser.callbacks[IRC_PARSER_STATE_NAME]      = &irc_onname;
    irc->parser.callbacks[IRC_PARSER_STATE_HOST]      = &irc_onhost;
    irc->parser.callbacks[IRC_PARSER_STATE_COMMAND]   = &irc_oncommand;
    irc->parser.callbacks[IRC_PARSER_STATE_PARMETERS] = &irc_onparam;
    irc->parser.callbacks[IRC_PARSER_STATE_END]       = &irc_onend;
    irc->parser.callbacks[IRC_PARSER_STATE_ERROR]     = &irc_onerror;

    return irc;

error:
    free(irc->name);
    free(irc->nick);
    free(irc->pattern);
    free(irc->auth);

    if (irc->database)
        database_destroy(irc->database);
    return NULL;
}

bool irc_modules_reload(irc_t *irc, const char *name) {
    return module_manager_module_reload(irc->moduleman, name);
}

bool irc_modules_add(irc_t *irc, const char *name) {
    string_t *error  = NULL;
    module_t *module = NULL;

    if (strstr(name, "//") || strstr(name, "./"))
        return false;

    string_t *file = string_create("modules/");
    string_catf(file, "%s.so", name);

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

static bool irc_channel_find(const void *a, const void *b) {
    return !strcmp((const char *)a, (const char *)b);
}

bool irc_channels_add(irc_t *irc, const char *channel) {
    if (list_search(irc->channels, &irc_channel_find, channel)) {
        printf("    channel  => %s already exists\n", channel);
        return false;
    }

    list_push(irc->channels, strdup(channel));
    printf("    channel  => %s added\n", channel);
    return true;
}

void irc_destroy(irc_t *irc, sock_restart_t *restart, char **name) {
    if (irc->sock && !restart)
        irc_quit_raw(irc, NULL, "Shutting down");

    if (name)
        *name = strdup(irc->name);

    irc_queue_destroy(irc);
    database_destroy(irc->database);
    regexpr_cache_destroy(irc->regexprcache);
    module_manager_destroy(irc->moduleman);

    // destroy channels and messages
    list_iterator_t *it;
    for (it = list_iterator_create(irc->channels); !list_iterator_end(it); )
        free(list_iterator_next(it));
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
    return true;
}

int irc_process(irc_t *irc, void *data) {
    char buffer[513];
    int  read;

    if ((read = sock_recv(irc->sock, buffer, sizeof(buffer))) == -1)
        return -1;

    irc_parser_next(&irc->parser, buffer, read);

    /*
     * Don't process commands unless the state of the line parser itself
     * has reached the end.
     */
    if (!(irc->flags & IRC_STATE_END))
        return read;

    /* now deal with commands */
    if (!(irc->message.channel && !strncmp(irc->message.content, irc->pattern, strlen(irc->pattern)))) {
        irc->flags &= ~IRC_STATE_END;
        return read;
    }

    char *beg = irc->message.content + strlen(irc->pattern);
    char *end = strchr(beg, ' ');

    if (end)
        *end = 0;

    module_t *find = module_manager_module_command(irc->moduleman, beg);
    if (!find) {
        irc_write(irc, irc->message.nick, "Sorry, there is no command named %s available. I do however, take requests if asked nicely.", beg);
        irc->flags &= ~IRC_STATE_END;
        return read;
    }

    if (end && end[1]) {
        beg = &end[1];
        end = beg + strlen(beg) - 1;
        while (end > beg && isspace(*end))
            end--;
        end[1] = 0;
        end    = beg;
    }

    cmd_channel_push(data, cmd_entry_create(data, find, irc->message.channel, irc->message.nick, end));
    irc->flags &= ~IRC_STATE_END;
    return read;
}
