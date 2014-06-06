#include "irc.h"
#include "sock.h"
#include "module.h"
#include "command.h"
#include "ircman.h"
#include "access.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include <ctype.h>

static const char *irc_target_nick(const char *target);
static const char *irc_target_host(const char *target);

/* Raw communication protocol */
typedef int (*irc_func_standard_t)(irc_t *, const char *);
typedef int (*irc_func_extended_t)(irc_t *, const char *, const char *);

typedef struct {
    const size_t        baselen; /* Base amount of bytes to represent an empty func */
    irc_func_standard_t standard;
    irc_func_extended_t extended;
} irc_command_func_t;

typedef enum {
    IRC_COMMAND_JOIN,
    IRC_COMMAND_PART,
    IRC_COMMAND_QUIT,
    IRC_COMMAND_WRITE,
    IRC_COMMAND_ACTION
} irc_command_t;

int irc_join_raw(irc_t *irc, const char *channel) {
    return sock_sendf(irc->sock, "JOIN %s\r\n", channel);
}

int irc_part_raw(irc_t *irc, const char *channel) {
    return sock_sendf(irc->sock, "PART %s\r\n", channel);
}

int irc_quit_raw(irc_t *irc, const char *message) {
    return sock_sendf(irc->sock, "QUIT :%s\r\n", message);
}

int irc_write_raw(irc_t *irc, const char *target, const char *message) {
    return sock_sendf(irc->sock, "PRIVMSG %s :%s\r\n", target, message);
}

int irc_action_raw(irc_t *irc, const char *target, const char *action) {
    return sock_sendf(irc->sock, "PRIVMSG %s :\001ACTION %s\001\r\n", target, action);
}

static const irc_command_func_t irc_commands[] = {
    [IRC_COMMAND_JOIN]   = { 0,  &irc_join_raw, NULL            },
    [IRC_COMMAND_PART]   = { 0,  &irc_part_raw, NULL            },
    [IRC_COMMAND_QUIT]   = { 0,  &irc_quit_raw, NULL            },
    [IRC_COMMAND_WRITE]  = { 12, NULL,          &irc_write_raw  },
    [IRC_COMMAND_ACTION] = { 22, NULL,          &irc_action_raw }
};

/* Colors (come first for the buffered protocol) */
static const char *irc_colors[] = {
    "WHITE",    "BLACK",   "DARKBLUE", "DARKGREEN", "RED",  "BROWN",
    "PURPLE",   "OLIVE",   "YELLOW",   "GREEN",     "TEAL", "CYAN",
    "BLUE",     "MAGENTA", "DARKGRAY", "LIGHTGRAY"
};

static size_t irc_color_lookup(const char *color) {
    for (size_t i = 0; i < sizeof(irc_colors)/sizeof(*irc_colors); i++)
        if (!strcmp(irc_colors[i], color))
            return i;
    return 15;
}

static string_t *irc_color_parse_code(const char *source) {
    const char *p1;
    const char *p2;
    const char *cur    = source;
    string_t   *string = string_construct();

    while ((p1 = strchr(cur, '['))) {
        const char *replaced = NULL;
        /* Suitable tag found */
        if (p1[1] != '\0' && (p2 = strchr(p1, ']')) && (p2 - p1) > 1 && (p2 - p1) < 31) {
            /* Extract the tag */
            size_t taglen = p2 - p1 - 1;
            char tagdata[32];
            memcpy(tagdata, p1 + 1, taglen);
            tagdata[taglen] = '\0';
            /* Termination */
            if (!strcmp(tagdata, "/COLOR"))
                replaced = "\x0F";
            else if (strstr(tagdata, "COLOR=") == tagdata) {
                /* Parse the color line */
                int fgc;
                int bgc = -2;
                /* Background color */
                char *find = strchr(tagdata + 6, '/');
                if (find) {
                    *find++ = '\0';
                    bgc = irc_color_lookup(find);
                }
                /* Foreground color */
                fgc = irc_color_lookup(tagdata + 6);
                if (fgc != -1 && bgc == -2) {
                    snprintf(tagdata, sizeof(tagdata), "\x03%02d", fgc);
                    replaced = tagdata;
                } else if (fgc != -1 && bgc >= 0) {
                    snprintf(tagdata, sizeof(tagdata), "\x03%02d,%02d", fgc, bgc);
                    replaced = tagdata;
                }
            }
            /* Deal with the other tags */
            else if (!strcmp(tagdata, "B") || !strcmp(tagdata, "/B")) replaced = "\x02";
            else if (!strcmp(tagdata, "U") || !strcmp(tagdata, "/U")) replaced = "\x1F";
            else if (!strcmp(tagdata, "I") || !strcmp(tagdata, "/I")) replaced = "\x16";
        }
        if (replaced) {
            string_catf(string, "%.*s%s", (int)(p1 - cur), cur, replaced);
            cur = p2 + 1;
        } else {
            if (!p2) p2 = cur + strlen(cur);
            string_catf(string, "%.*s", (int)(p2 - cur + 1), cur);
            cur = p2 + 1;
        }
    }
    string_catf(string, "%s", cur);
    return string;
}

/* Buffered protocol */
typedef struct {
    string_t     *target;
    string_t     *payload;
    irc_command_t command;
} irc_queued_t;

static void irc_enqueue_standard(irc_t *irc, const char *target, irc_command_t command) {
    irc_queued_t *entry = malloc(sizeof(*entry));

    entry->target  = string_create(target);
    entry->payload = NULL;
    entry->command = command;

    list_push(irc->queue, entry);
}

static void irc_enqueue_extended(irc_t *irc, const char *target, string_t *payload, irc_command_t command) {
    irc_queued_t *entry    = malloc(sizeof(*entry));
    const char   *contents = string_contents(payload);

    string_reassociate(payload, irc_color_parse_code(contents));

    entry->target  = string_create(target);
    entry->payload = payload;
    entry->command = command;

    list_push(irc->queue, entry);
}

void irc_unqueue(irc_t *irc) {
    irc_queued_t *entry;
    size_t        events = 0;

    /* Only when enough time has passed */
    if (difftime(time(NULL), irc->lastunqueue) <= IRC_FLOOD_INTERVAL) {
        irc_manager_wake(irc->manager);
        return;
    }

    while (events != IRC_FLOOD_LINES && (entry = list_shift(irc->queue))) {
        const size_t              targetlen = string_length(entry->target);
        const char               *target    = string_contents(entry->target);
        const irc_command_func_t *func      = &irc_commands[entry->command];

        /* If there is a payload we use the extended call */
        if (entry->payload) {
            size_t      payloadlen = string_length(entry->payload);
            const char *payload    = string_contents(entry->payload);
            size_t      corelen    = func->baselen + targetlen + 63; /* 63 is MAX_HOST_LENGTH */

            /* Split payload for 512 byte IRC line limit */
            while (corelen + payloadlen > 512) {
                char truncate[512];
                size_t size = sizeof(truncate) - corelen;
                strncpy(truncate, payload, size);
                truncate[size] = '\0';
                func->extended(irc, target, truncate);
                /* Flood protection */
                if (++events == IRC_FLOOD_LINES) {
                    /* Construct a new partial payload */
                    char *move = string_move(entry->payload);
                    entry->payload = string_create(move + (payload - move) + size);
                    free(move);
                    list_prepend(irc->queue, entry);
                    break;
                }
                payloadlen -= size;
                payload += size;
            }
            func->extended(irc, target, payload);
            events++;
            string_destroy(entry->payload);
        } else {
            /* Otherwise we do a standard call */
            func->standard(irc, target);
            events++;
        }
        string_destroy(entry->target);
        free(entry);
    }

    /* Flood protection */
    if (events == IRC_FLOOD_LINES) {
        irc->lastunqueue = time(NULL);
        irc_manager_wake(irc->manager);
    }
}

void irc_quit(irc_t *irc, const char *message) {
    irc_enqueue_standard(irc, message, IRC_COMMAND_QUIT);
}

void irc_join(irc_t *irc, const char *channel) {
    irc_enqueue_standard(irc, channel, IRC_COMMAND_JOIN);
    /* TODO: fix */
}

static void irc_channel_destroy(irc_channel_t *channel);
void irc_part(irc_t *irc, const char *channel) {
    irc_channel_t *chan = hashtable_find(irc->channels, channel);
    irc_channel_destroy(chan);
    hashtable_remove(irc->channels, channel);

    irc_enqueue_standard(irc, channel, IRC_COMMAND_PART);
}

void irc_action(irc_t *irc, const char *channel, const char *fmt, ...) {
    va_list  va;
    va_start(va, fmt);
    irc_enqueue_extended(irc, channel, string_vformat(fmt, va), IRC_COMMAND_ACTION);
    va_end(va);
}

void irc_write(irc_t *irc, const char *channel, const char *fmt, ...) {
    va_list  va;
    va_start(va, fmt);
    irc_enqueue_extended(irc, channel, string_vformat(fmt, va), IRC_COMMAND_WRITE);
    va_end(va);
}

/* Channel management */
static void irc_user_destroy(irc_user_t *user);
static void irc_module_destroy(irc_module_t *module);
static void irc_channel_destroy(irc_channel_t *channel) {
    free(channel->channel);
    free(channel->topic);
    hashtable_foreach(channel->users, NULL, &irc_user_destroy);
    hashtable_destroy(channel->users);
    hashtable_foreach(channel->modules, NULL, &irc_module_destroy);
    hashtable_destroy(channel->modules);
    free(channel);
}

static void irc_channel_join(irc_channel_t *channel, irc_t *irc) {
    irc_join_raw(irc, channel->channel);
}

static void irc_channels_join(irc_t *irc) {
    hashtable_foreach(irc->channels, irc, &irc_channel_join);
}

/* This should be the same in modules/module.h */
typedef enum {
    MODULE_STATUS_REFERENCED,
    MODULE_STATUS_SUCCESS,
    MODULE_STATUS_FAILURE,
    MODULE_STATUS_ALREADY,
    MODULE_STATUS_NONEXIST
} module_status_t;

module_status_t irc_modules_add(irc_t *irc, const char *name) {
    string_t *error  = NULL;
    module_t *module = NULL;

    if (strstr(name, "//") || strstr(name, "./"))
        return MODULE_STATUS_FAILURE;

    string_t *file = string_format("modules/%s.so", name);
    if ((module = module_manager_module_search(irc->moduleman, string_contents(file), MMSEARCH_FILE))) {
        printf("    module   => %s [%s] already loaded\n", module->name, name);
        string_destroy(file);
        return MODULE_STATUS_ALREADY;
    }

    if ((module = module_open(string_contents(file), irc->moduleman, &error))) {
        printf("    module   => %s [%s] loaded\n", module->name, module->file);
        string_destroy(file);
        return MODULE_STATUS_SUCCESS;
    }

    if (error) {
        printf("    module   => %s loading failed (%s)\n", name, string_contents(error));
        string_destroy(error);
    } else {
        printf("    module   => %s loading failed\n", name);
    }

    string_destroy(file);
    return MODULE_STATUS_FAILURE;
}

/* Module config copy */
static irc_module_t *irc_module_create(config_module_t *module) {
    irc_module_t *mod = malloc(sizeof(*mod));
    mod->module = strdup(module->name);
    mod->kvs    = hashtable_copy(module->kvs, &strdup);
    return mod;
}

static void irc_module_destroy(irc_module_t *module) {
    hashtable_foreach(module->kvs, NULL, &free);
    hashtable_destroy(module->kvs);
    free(module->module);
    free(module);
}

static void irc_channels_add_module(config_module_t *module, irc_channel_t *channel) {
    /* Try loading it if it isn't loaded */
    irc_modules_add(channel->instance, module->name);
    hashtable_insert(channel->modules, module->name, irc_module_create(module));
}

bool irc_channels_add(irc_t *irc, config_channel_t *channel) {
    if (hashtable_find(irc->channels, channel->name)) {
        printf("    channel  => %s already exists\n", channel->name);
        return false;
    }

    irc_channel_t *chan = malloc(sizeof(*chan));

    chan->users    = hashtable_create(32);
    chan->channel  = strdup(channel->name);
    chan->topic    = NULL;
    chan->modules  = hashtable_create(32);
    chan->instance = irc;

    /* Deeply copy the config_channel_t modules hashtable and configuration
     * into an irc_channel_t + irc_module_t hashtable.
     */
    hashtable_foreach(channel->modules, chan, &irc_channels_add_module);
    hashtable_insert(irc->channels, channel->name, chan);

    printf("    channel  => %s added\n", channel->name);
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

    if (hashtable_find(chan->users, nick))
        return;

    hashtable_insert(chan->users, nick, irc_user_create(nick, host));
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
irc_t *irc_create(config_instance_t *instance) {
    irc_t *irc = malloc(sizeof(irc_t));

    irc->name            = strdup(instance->name);
    irc->nick            = strdup(instance->nick);
    irc->pattern         = strdup(instance->pattern);
    irc->auth            = (instance->auth) ? strdup(instance->auth) : NULL;
    irc->ready           = false;
    irc->identified      = false;
    irc->channels        = hashtable_create(64);
    irc->queue           = list_create();
    irc->database        = database_create(instance->database);
    irc->regexprcache    = regexpr_cache_create();
    irc->moduleman       = module_manager_create(irc);
    irc->lastunqueue     = 0;

    memset(&irc->message, 0, sizeof(irc_message_t));

    irc->buffer.data[0]  = '\0';
    irc->buffer.offset   = 0;

    printf("instance: %s\n", irc->name);
    printf("    nick     => %s\n", irc->nick);
    printf("    pattern  => %s\n", irc->pattern);
    printf("    auth     => %s\n", (irc->auth) ? irc->auth : "(None)");
    printf("    database => %s\n", instance->database);
    printf("    host     => %s\n", instance->host);
    printf("    port     => %s\n", instance->port);
    printf("    ssl      => %s\n", instance->ssl ? "(Yes)" : "(No)");

    return irc;
}

void irc_destroy(irc_t *irc, sock_restart_t *restart, char **name) {
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
    free(irc->message.nick);
    free(irc->message.host);
    free(irc->message.channel);
    free(irc->message.content);

    sock_destroy(irc->sock, restart);

    free(irc);
}

/* Module management */
static bool irc_modules_exists(irc_t *irc, const char *name) {
    list_iterator_t *it = list_iterator_create(irc->moduleman->modules);
    while (!list_iterator_end(it)) {
        if (!strcmp(((module_t*)list_iterator_next(it))->name, name)) {
            list_iterator_destroy(it);
            return true;
        }
    }
    list_iterator_destroy(it);
    return false;
}

typedef struct {
    const char *exclude;
    const char *module;
    size_t      count;
} irc_module_ref_t;

static void irc_modules_refs_modules(const char *module, irc_module_ref_t *refs) {
    if (!strcmp(module, refs->module))
        refs->count++;
}

static void irc_modules_refs_channels(irc_channel_t *channel, irc_module_ref_t *refs) {
    /* We exclude the channel we come from as a reference */
    if (!strcmp(refs->exclude, channel->channel))
        return;
    hashtable_foreach(channel->modules, refs, &irc_modules_refs_modules);
}

static size_t irc_modules_refs(irc_t *irc, const char *module, const char *exclude) {
    /* Will exclude `exclude' from the reference search */
    irc_module_ref_t refs = {
        .module  = module,
        .exclude = exclude,
        .count   = 0
    };
    hashtable_foreach(irc->channels, &refs, &irc_modules_refs_channels);
    return refs.count;
}
module_status_t irc_modules_reload(irc_t *irc, const char *name) {
    /* Reloading a module reloads it everywhere */
    if (!module_manager_module_reload(irc->moduleman, name))
        return MODULE_STATUS_FAILURE;
    return MODULE_STATUS_SUCCESS;
}

module_status_t irc_modules_unload(irc_t *irc, const char *channel, const char *module, bool force) {
    size_t refs = irc_modules_refs(irc, module, channel);
    if (refs != 0)
        return MODULE_STATUS_REFERENCED;
    if (!module_manager_module_unload(irc->moduleman, module))
        return MODULE_STATUS_FAILURE;
    return MODULE_STATUS_SUCCESS;
}

module_status_t irc_modules_disable(irc_t *irc, const char *chan, const char *name) {
    irc_channel_t *channel = hashtable_find(irc->channels, chan);
    if (hashtable_find(channel->modules, name)) {
        return hashtable_remove(channel->modules, name)
                   ? MODULE_STATUS_SUCCESS
                   : MODULE_STATUS_FAILURE;
    }
    if (irc_modules_exists(irc, name))
        return MODULE_STATUS_ALREADY;
    return MODULE_STATUS_NONEXIST;
}

module_status_t irc_modules_enable(irc_t *irc, const char *chan, const char *name) {
    irc_channel_t *ch = hashtable_find(irc->channels, chan);
    if (hashtable_find(ch->modules, name))
        return MODULE_STATUS_ALREADY;
    if (!irc_modules_exists(irc, name))
        return MODULE_STATUS_NONEXIST;

    /* Everytime a module is enabled we have to load the configuration file and
     * find the appropriate information for it.s
     */
    list_t            *config   = config_load("config.ini");
    config_instance_t *instance = config_instance_find(config, irc->name);
    config_channel_t  *channel  = config_channel_find(instance, chan);
    config_module_t   *module   = config_module_find(channel, name);
    hashtable_insert(ch->modules, name, irc_module_create(module));

    config_unload(config);
    return MODULE_STATUS_SUCCESS;
}

static bool irc_lexicographical_sort(const void *a, const void *b) {
    return strcmp(a, b) >= 0;
}

static void irc_modules_channel_create(irc_module_t *module, list_t *list) {
    list_push(list, module->module);
}

/* The one function gets all loaded modules, while the other shows all the
 * modules on the given channel.
 */
list_t *irc_modules_loaded(irc_t *irc) {
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

list_t *irc_modules_enabled(irc_t *irc, const char *chan) {
    irc_channel_t *channel = hashtable_find(irc->channels, chan);
    if (!channel)
        return NULL;
    list_t *list = list_create();
    hashtable_foreach(channel->modules, list, &irc_modules_channel_create);
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
    size_t length = split ? (size_t)(split - target) : strlen(target);
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
        sock_sendf(irc->sock, "PONG :%s\r\n", params[0]);
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
        } else if (numeric == ERR_NICKNAMEINUSE) {
            /* Change nickname by appending a tail */
            string_t *tail = string_format("%s_", irc->nick);
            free(irc->nick);
            irc->nick = string_move(tail);
            sock_sendf(irc->sock, "NICK %s\r\n", irc->nick);
            return;
        }
        return;
    }

    if (!strncmp(command, "PRIVMSG", end - command) && params[1]) {
        free(irc->message.nick);
        free(irc->message.host);
        free(irc->message.channel);
        free(irc->message.content);
        irc->message.nick    = strdup(irc_target_nick(prefix));
        irc->message.host    = strdup(irc_target_host(prefix));
        irc->message.channel = strdup(params[0]);
        irc->message.content = strdup(params[1]);

        /* The bot ignores anyone who is -1 */
        if (access_ignore(irc, irc->message.nick))
            return;
        /* It also ignores itself */
        if (!strcmp(irc->message.nick, irc->nick))
            return;

        /* Trim trailing whitespace */
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
                irc_write(irc, irc_target_nick(prefix),
                    "Sorry, there is no command named %s available. I do however, take requests if asked nicely.", skip);
                return;
            }

            /* If the channel doesn't have this module enabled then don't bother */
            irc_channel_t *channel = hashtable_find(irc->channels, irc->message.channel);

            /* We may not be coming from a channel */
            if (channel) {
                /* If the channel doesn't have the module we don't bother */
                if (!hashtable_find(channel->modules, skip))
                    return;
            }

            /* Skip the initial part of the module */
            char *next = irc->message.content + strlen(irc->pattern) + strlen(skip);
            while (isspace(*next))
                next++;

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
        irc_destroy(irc, SOCK_RESTART_NIL, NULL);
    } else if (!strncmp(command, "JOIN", end - command)) {
        if (access_shitlist(irc, prefix)) {
            sock_sendf(irc->sock, "KICK %s :you are banned\r\n", params[0]);
            /* TODO: ban */
            return;
        }
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

    char temp[256];
    int read;
    if ((read = sock_recv(irc->sock, temp, sizeof(temp) - 1)) <= 0)
        return;

    temp[read] = '\0';
    for (int i = 0; i < read; ++i) {
        /* Ignore MIRC characters */
        if (temp[i] == '\r')   continue;
        if (temp[i] == '\x02') continue; /* bold      */
        if (temp[i] == '\x1f') continue; /* underline */
        if (temp[i] == '\x16') continue; /* reverse   */
        if (temp[i] == '\x0f') continue; /* reset     */

        /* Dealing with color control */
        int color = 0;
        if (temp[i] == '\x03') {
            color = !(color & 1);
            continue;
        }
        if ((color & 1) && isdigit(temp[i])) { color |= 2; continue; }
        if ((color & 7) && isdigit(temp[i])) { color |= 8; continue; }
        if ((color & 3) && temp[i] == ',') {
            color |= 4;
            continue;
        }

        if (temp[i] == '\n') {
            irc->buffer.data[irc->buffer.offset] = '\0';
            irc->buffer.offset                   = 0;
            irc_parse(irc, data);
        } else {
            irc->buffer.data[irc->buffer.offset++] = temp[i];
            if (irc->buffer.offset > sizeof(irc->buffer.data) - 1)
                raise(SIGUSR1);
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

const char *irc_pattern(irc_t *irc, const char *newpattern) {
    if (newpattern) {
        free(irc->pattern);
        irc->pattern = strdup(newpattern);
    }
    return irc->pattern;
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
