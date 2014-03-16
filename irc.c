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

static const char *irc_target_nick(const char *target);
static const char *irc_target_host(const char *target);

/* Raw communication protocol */
int irc_join_raw(irc_t *irc, const char *channel) {
    return sock_sendf(irc->sock, "JOIN %s\r\n", channel);
}

int irc_part_raw(irc_t *irc, const char *channel) {
    return sock_sendf(irc->sock, "PART %s\r\n", channel);
}

int irc_quit_raw(irc_t *irc, const char *message) {
    if (message)
        return sock_sendf(irc->sock, "QUIT :%s\r\n", message);
    return sock_sendf(irc->sock, "QUIT\r\n");
}

int irc_write_raw(irc_t *irc, const char *target, const char *message) {
    return sock_sendf(irc->sock, "PRIVMSG %s :%s\r\n", target, message);
}

int irc_action_raw(irc_t *irc, const char *target, const char *action) {
    return sock_sendf(irc->sock, "PRIVMSG %s :\001ACTION %s\001\r\n", target, action);
}

/* Buffered protocol */
typedef int (*irc_queue_func_standard_t)(irc_t *, const char *);
typedef int (*irc_queue_func_extended_t)(irc_t *, const char *, const char *);
typedef struct {
    irc_t    *irc;
    string_t *target;
    string_t *payload;
    union {
        irc_queue_func_standard_t standard;
        irc_queue_func_extended_t extended;
    };
} irc_queued_t;

static void irc_enqueue_standard(irc_t *irc, const char *target, irc_queue_func_standard_t callback) {
    irc_queued_t *entry = malloc(sizeof(*entry));

    entry->irc      = irc;
    entry->target   = string_create(target);
    entry->payload  = NULL;
    entry->standard = callback;

    list_push(irc->queue, entry);
}

static void irc_enqueue_extended(irc_t *irc, const char *target, string_t *payload, irc_queue_func_extended_t callback) {
    irc_queued_t *entry = malloc(sizeof(*entry));

    entry->irc      = irc;
    entry->target   = string_create(target);
    entry->payload  = payload;
    entry->extended = callback;

    list_push(irc->queue, entry);
}

void irc_unqueue(irc_t *irc) {
    irc_queued_t *entry;
    while ((entry = list_shift(irc->queue))) {
        const char *target = string_contents(entry->target);
        /* If there is a payload we use the extended call */
        if (entry->payload) {
            const char *payload = string_contents(entry->payload);
            entry->extended(entry->irc, target, payload);
            string_destroy(entry->payload);
        } else {
            /* Otherwise we do a standard call */
            entry->standard(entry->irc, target);
        }
        string_destroy(entry->target);
        free(entry);
    }
}

void irc_quit(irc_t *irc, const char *message) {
    irc_enqueue_standard(irc, message, &irc_quit_raw);
}

void irc_join(irc_t *irc, const char *channel) {
    irc_channels_add(irc, channel);
    irc_enqueue_standard(irc, channel, &irc_join_raw);
}

void irc_part(irc_t *irc, const char *channel) {
    hashtable_remove(irc->channels, channel);
    irc_enqueue_standard(irc, channel, &irc_part_raw);
}

void irc_action(irc_t *irc, const char *channel, const char *fmt, ...) {
    va_list  va;
    va_start(va, fmt);
    irc_enqueue_extended(irc, channel, string_vformat(fmt, va), &irc_action_raw);
    va_end(va);
}

void irc_write(irc_t *irc, const char *channel, const char *fmt, ...) {
    va_list  va;
    va_start(va, fmt);
    irc_enqueue_extended(irc, channel, string_vformat(fmt, va), &irc_write_raw);
    va_end(va);
}

/* Channel management */
static void irc_user_destroy(irc_user_t *user);
static void irc_channel_destroy(irc_channel_t *channel) {
    free(channel->channel);
    free(channel->topic);
    hashtable_foreach(channel->users, NULL, &irc_user_destroy);
    hashtable_destroy(channel->users);
    free(channel);
}

static void irc_channel_join(irc_channel_t *channel, irc_t *irc) {
    irc_join_raw(irc, channel->channel);
}

static void irc_channels_join(irc_t *irc) {
    hashtable_foreach(irc->channels, irc, &irc_channel_join);
}

bool irc_channels_add(irc_t *irc, const char *channel) {
    if (hashtable_find(irc->channels, channel)) {
        printf("    channel  => %s already exists\n", channel);
        return false;
    }

    irc_channel_t *chan = malloc(sizeof(*chan));
    chan->users   = hashtable_create(32);
    chan->channel = strdup(channel);
    chan->topic   = NULL;

    hashtable_insert(irc->channels, channel, chan);
    printf("    channel  => %s added\n", channel);
    return true;
}

/* User management */
static void irc_user_destroy(irc_user_t *user) {
    free(user->nick);
    free(user->host);
    free(user);
}

static irc_user_t *irc_user_create(const char *nick, const char *host) {
    irc_user_t *user = malloc(sizeof(*user));
    user->nick = strdup(nick);
    user->host = strdup(host);
    return user;
}

void irc_users_insert(irc_t *irc, const char *channel, const char *prefix) {
    irc_channel_t *chan = hashtable_find(irc->channels, channel);
    if (!chan) return;

    const char *nick = irc_target_nick(prefix);
    const char *host = irc_target_host(prefix);
    irc_user_t *user = irc_user_create(nick, host);

    /* Don't insert it again if it already exists */
    if (!hashtable_find(chan->users, nick))
        hashtable_insert(chan->users, nick, user);
}

void irc_users_remove(irc_t *irc, const char *channel, const char *prefix) {
    irc_channel_t *chan = hashtable_find(irc->channels, channel);
    if (!chan) return;

    const char *nick = irc_target_nick(prefix);
    irc_user_t *user = hashtable_find(chan->users, nick);
    if (!user)
        return;

    irc_user_destroy(user);
    hashtable_remove(chan->users, nick);
}

/* Instance management */
irc_t *irc_create(config_t *entry) {
    irc_t *irc = malloc(sizeof(irc_t));

    irc->name            = strdup(entry->name);
    irc->nick            = strdup(entry->nick);
    irc->pattern         = strdup(entry->pattern);
    irc->auth            = (entry->auth) ? strdup(entry->auth) : NULL;
    irc->ready           = false;
    irc->identified      = false;
    irc->channels        = hashtable_create(64);
    irc->queue           = list_create();
    irc->database        = database_create(entry->database);
    irc->regexprcache    = regexpr_cache_create();
    irc->moduleman       = module_manager_create(irc);

    memset(&irc->message, 0, sizeof(irc_message_t));

    /* First realloc will make it the correct size */
    irc->buffer.data     = malloc(512);
    irc->buffer.offset   = 0;
    irc->buffer.size     = 512;

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

void irc_destroy(irc_t *irc, sock_restart_t *restart, char **name) {
    /* Process any left over things in the queue before destroying */
    irc_unqueue(irc);

    if (irc->sock && !restart)
        irc_quit_raw(irc, "Shutting down");

    if (name)
        *name = strdup(irc->name);

    database_destroy(irc->database);
    regexpr_cache_destroy(irc->regexprcache);
    module_manager_destroy(irc->moduleman);

    hashtable_foreach(irc->channels, NULL, &irc_channel_destroy);
    hashtable_destroy(irc->channels);
    list_destroy(irc->queue);

    free(irc->auth);
    free(irc->nick);
    free(irc->name);
    free(irc->pattern);
    free(irc->buffer.data);

    free(irc->message.nick);
    free(irc->message.host);
    free(irc->message.channel);
    free(irc->message.content);

    sock_destroy(irc->sock, restart);

    free(irc);
}

/* Module management */
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

static bool irc_lexicographical_sort(const void *a, const void *b) {
    return strcmp(a, b) >= 0;
}

list_t *irc_modules(irc_t *irc) {
    list_t          *list = list_create();
    list_iterator_t *it   = list_iterator_create(irc->moduleman->modules);
    while (!list_iterator_end(it)) {
        module_t *entry = list_iterator_next(it);
        list_push(list, (void *)entry->name);
    }
    list_iterator_destroy(it);
    list_sort(list, &irc_lexicographical_sort);
    return list;
}

/* Network management */
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
    irc->ready      = true;
    irc->identified = true;
    return true;
}

/* Parser */
static const char *irc_target_nick(const char *target) {
    static char buffer[128];

    char *split = strstr(target, "!");
    size_t length = split ? split - target : strlen(target);
    if (length > sizeof(buffer) - 1)
        length = sizeof(buffer) - 1;

    memcpy(buffer, target, length);
    buffer[length] = '\0';
    return buffer;
}

static const char *irc_target_host(const char *target) {
    static char buffer[128];
    const char *split = strstr(target, "!");
    if (!split)
        split = target;

    size_t length = strlen(split);
    if (length > sizeof(buffer) - 1)
        length = sizeof(buffer) - 1;

    memcpy(buffer, target, length);
    buffer[length] = '\0';
    return buffer;
}

static void irc_parse(irc_t *irc, void *data) {
    char *params[11];
    char *command = NULL;
    char *prefix  = NULL;
    char *parse   = irc->buffer.data;
    char *end     = irc->buffer.data + irc->buffer.offset;
    int   numeric = 0;

    /* <prefix> ::= <servername> | <nick> [ '!' <user> ] [ '@' <host> ] */
    if (irc->buffer.data[0] == ':') {
        while (*parse && *parse != ' ')
            parse++;
        *parse++ = '\0';
        prefix = irc->buffer.data + 1;
    }

    /* <command> ::= <letter> { <letter> } | <number> <number> <number> */
    if (isdigit(parse[0]) && isdigit(parse[1]) && isdigit(parse[2])) {
        parse[3] = '\0';
        numeric  = atoi(parse);
        parse   += 4;
    } else {
        command = parse;
        while (*parse && *parse != ' ')
            parse++;
        *parse++ = '\0';
    }

    /* <params> ::= <space> [ ':' <trailing> | <middle> <params> ] */
    for (size_t i = 0; i < sizeof(params)/sizeof(*params) - 1; ) {
        /* When starting from ':' it's the last parameter */
        if (*parse == ':') {
            params[i++] = parse + 1;
            break;
        }

        /*
         * Otherwise we have the middle to worry about:
         *  <middle> ::= <Any *non-empty* sequence of octets not including
         *               space or NUL or CR or LF, the first of which
         *               cannot be <space>>
         */
        params[i++] = parse;

        /*
         * <trailing> ::= <Any possibly *empty*, sequence of octets not
         *                 including NUL or CR or LF>
         */
        for (; *parse && *parse != ' '; parse++)
            ;

        /* Finished */
        if (!*parse)
            break;

        *parse++ = '\0';
    }

    /* Deal with server PING/PONG as early as possible */
    if (command && !strncmp(command, "PING", end - command) && params[0]) {
        sock_sendf(irc->sock, "PONG :%s", params[0]);
        return;
    }

    if (numeric) {
        /*
         * When the server sends the end of it's MOTD or it doesn't send a
         * MOTD at all we consider this the syncronized state and join the
         * channels we need to.
         */
        if ((numeric == RPL_ENDOFMOTD || numeric == ERR_NOMOTD))
            irc_channels_join(irc);

        else if (numeric == RPL_WELCOME) {
            irc->ready = true;
            printf("    irc      => ready\n");
            return;
        } else if (numeric == RPL_TOPIC) {
            irc_channel_t *channel = hashtable_find(irc->channels, params[1]);
            if (!channel)
                return;
            /* Update the channel topic */
            free(channel->topic);
            channel->topic = strdup(params[2]);
            return;
        } else if (numeric == RPL_NAMREPLY) {
            irc_channel_t *channel = hashtable_find(irc->channels, params[2]);
            if (!channel)
                return;
            /* Update the names */
            char *tokenize = strtok(params[3], " ");
            while (tokenize) {
                /* TODO: whois to get host */
                irc_users_insert(irc, channel->channel, tokenize);
                tokenize = strtok(NULL, " ");
            }
            return;
        }
        return;
    }

    if (!strncmp(command, "PRIVMSG", end - command) && params[1]) {
        /* Make a copy of the message for the always modules */
        free(irc->message.nick);
        free(irc->message.host);
        free(irc->message.channel);
        free(irc->message.content);
        irc->message.nick    = strdup(irc_target_nick(prefix));
        irc->message.host    = strdup(irc_target_host(prefix));
        irc->message.channel = strdup(params[0]);
        irc->message.content = strdup(params[1]);

        /* Trim trailing whitespace in message */
        char *trail = irc->message.content + strlen(irc->message.content) - 1;
        while (trail > irc->message.content && isspace(*trail))
            trail--;
        trail[1] = '\0';

        /* Did someone initiate a module? */
        if (!strncmp(params[1], irc->pattern, strlen(irc->pattern))) {
            /* Skip the pattern and strip the string */
            char *skip  = params[1] + strlen(irc->pattern);
            char *strip = strchr(skip, ' ');
            if (strip)
                *strip = '\0';

            /* Check for the appropriate module for this command */
            module_t *find = module_manager_module_command(irc->moduleman, skip);
            if (!find) {
                /* Couldn't find the module? */
                irc_write(irc, irc_target_nick(prefix),
                    "Sorry, there is no command named %s available. I do however, take requests if asked nicely.", skip);
                return;
            }

            /* Skip the initial part of the module */
            char *next = irc->message.content + strlen(irc->pattern) + strlen(skip);
            while (isspace(*next))
                next++;

            /*
             * Create a new command entry for the module and add it to the
             * command queue.
             */
            cmd_channel_push (
                data,
                cmd_entry_create (
                    data,
                    find,
                    params[0],
                    irc_target_nick(prefix),
                    next
                )
            );
        }
    } else if (!strncmp(command, "NOTICE", end - command) && params[1]) {
        /*
         * This is a special case to support Atheme's nick services.
         * NickServ sends a NOTICE containing "This nickname is registered",
         * we check for that and PRIVMSG NickServ to authenticate using
         * the authentication syntax. NickServ will send a NOTICE when
         * we're authenticated. For all other NOTICEs from NickServ we
         * simply ignore them.
         */
        if (!strcmp(irc_target_nick(prefix), "NickServ") && irc->auth) {
            if (strstr(params[1], "You are now identified")) {
                printf("    irc      => authenticated\n");
            } else if (strstr(params[1], "This nickname is registered")) {
                sock_sendf(irc->sock, "PRIVMSG NickServ :IDENTIFY %s %s\r\n", irc->nick, irc->auth);
            }
        }
    } else if (!strncmp(command, "KILL", end - command)) {
        /* If the IRCd wants to kill us for what ever reason, allow it. */
        irc_destroy(irc, SOCK_RESTART_NIL, NULL);
    } else if (!strncmp(command, "JOIN", end - command)) {
        irc_users_insert(irc, params[0], prefix);
    } else if (!strncmp(command, "PART", end - command)) {
        irc_users_remove(irc, params[0], prefix);
    }
}

void irc_process(irc_t *irc, void *data) {
    if (!irc->sock)
        return;

    if (!irc->ready && !irc->identified) {
        /*
         * RFC 1459 mentions that hostname and servername are typically
         * ignored by the IRCd when the USER command comes directly from a
         * connected client for security reasons. We simply ignore sending
         * those fields.
         */
        sock_sendf(irc->sock, "NICK %s\r\nUSER %s localhost 0 :redorito\r\n", irc->nick, irc->nick);
        irc->identified = true;
    }

    /* Read until we can't anymore */
    char ch = '\0';
    while (sock_recv(irc->sock, &ch, 1) != -1) {
        irc->buffer.data[irc->buffer.offset++] = ch;
        if (irc->buffer.offset > irc->buffer.size) {
            irc->buffer.size += 512;
            irc->buffer.data  = realloc(irc->buffer.data, irc->buffer.size);
        }
        /* A newline indicates that we can parse */
        if (ch == '\n') {
            /* Some IRCd only send \n instead of \r\n */
            irc->buffer.data[irc->buffer.offset - 1] = '\0';
            /* For the ones which don't we can terminate earlier */
            if (irc->buffer.data[irc->buffer.offset - 2] == '\r')
                irc->buffer.data[irc->buffer.offset - 2] = '\0';

            irc_parse(irc, data);
            irc->buffer.offset = 0;
            break;
        }
    }
}

/* Exposed functionality for the module API */
const char *irc_nick(irc_t *irc) {
    return irc->nick;
}

const char *irc_name(irc_t *irc) {
    return irc->name;
}

const char *irc_topic(irc_t *irc, const char *channel) {
    irc_channel_t *chan = hashtable_find(irc->channels, channel);
    return (chan) ? chan->topic : "(No topic)";
}

static void irc_users_callback(irc_user_t *user, list_t *list) {
    list_push(list, user->nick);
}

list_t *irc_users(irc_t *irc, const char *channel) {
    irc_channel_t *chan = hashtable_find(irc->channels, channel);
    if (!chan) return NULL;

    list_t *list = list_create();
    hashtable_foreach(chan->users, list, &irc_users_callback);
    list_sort(list, &irc_lexicographical_sort);
    return list;
}

static void irc_channels_callback(irc_channel_t *channel, list_t *list) {
    list_push(list, channel->channel);
}

list_t *irc_channels(irc_t *irc) {
    list_t *list = list_create();
    hashtable_foreach(irc->channels, list, &irc_channels_callback);
    list_sort(list, &irc_lexicographical_sort);
    return list;
}
