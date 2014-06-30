#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "irc.h"
#include "command.h"
#include "ircman.h"
#include "access.h"

#define isdigit(a) (((unsigned)(a)-'0') < 10)
#define isspace(a) ({ int c = (a); !!((c >= '\t' && c <= '\r') || c == ' '); })

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
    const char *p1     = NULL;
    const char *p2     = NULL;
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

static void irc_channel_destroy(irc_channel_t *channel);
static void irc_user_destroy(irc_user_t *user);
static void irc_module_destroy(irc_module_t *module);
static void irc_module_destroy(irc_module_t *module);
static irc_module_t *irc_module_create(config_module_t *module);

void irc_quit(irc_t *irc, const char *message) {
    irc_enqueue_standard(irc, message, IRC_COMMAND_QUIT);
}

void irc_join(irc_t *irc, const char *channel) {
    irc_enqueue_standard(irc, channel, IRC_COMMAND_JOIN);
    /*
     * Note: casting away const is fine in this context, these objects
     * are copied from with irc_channels_add.
     */
    config_channel_t chan = {
        .name    = (char *)channel,
        .modules = hashtable_create(8)
    };
    /* Channels joined from IRC will only have the following three modules enabled
     * by default.
     *
     * The hashtable is required for configuration options on the module. It's
     * deeply copied in irc_module_create. We delete them later. This is a hack.
     */
    config_module_t modules[3] = {
        { .name = (char *)"system", .kvs = hashtable_create(1) },
        { .name = (char *)"access", .kvs = hashtable_create(1) },
        { .name = (char *)"module", .kvs = hashtable_create(1) }
    };

    for (size_t i = 0; i < sizeof(modules)/sizeof(*modules); i++)
        hashtable_insert(chan.modules, modules[i].name, &modules[i]);

    irc_channels_add(irc, &chan);

    for (size_t i = 0; i < sizeof(modules)/sizeof(*modules); i++)
        hashtable_destroy(modules[i].kvs);

    hashtable_destroy(chan.modules);
}

void irc_part(irc_t *irc, const char *channel) {
    irc_channel_t *chan = hashtable_find(irc->channels, channel);
    if (!chan)
        return;

    irc_channel_destroy(chan);
    hashtable_remove(irc->channels, channel);
    irc_enqueue_standard(irc, channel, IRC_COMMAND_PART);
}

void irc_actionv(irc_t *irc, const char *channel, const char *fmt, va_list ap) {
    va_list va;
    va_copy(va, ap);
    irc_enqueue_extended(irc, channel, string_vformat(fmt, va), IRC_COMMAND_ACTION);
    va_end(va);
}

void irc_writev(irc_t *irc, const char *channel, const char *fmt, va_list ap) {
    va_list va;
    va_copy(va, ap);
    irc_enqueue_extended(irc, channel, string_vformat(fmt, va), IRC_COMMAND_WRITE);
    va_end(va);
}

void irc_action(irc_t *irc, const char *channel, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    irc_actionv(irc, channel, fmt, va);
    va_end(va);
}

void irc_write(irc_t *irc, const char *channel, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    irc_writev(irc, channel, fmt, va);
    va_end(va);
}

/* Message management */
static void irc_message_destroy(irc_message_t *message) {
    free(message->nick);
    free(message->host);
    free(message->content);
}

#define irc_message_change(MESSAGE, NICK, HOST, CONTENT) \
    (((MESSAGE)->nick    = (NICK)),                      \
     ((MESSAGE)->host    = (HOST)),                      \
     ((MESSAGE)->content = (CONTENT)))

static void irc_message_update(irc_message_t *message, const char *prefix, const char *content) {
    irc_message_destroy(message);
    irc_message_change(message, strdup(irc_target_nick(prefix)),
                                strdup(irc_target_host(prefix)),
                                strdup(content));
}

void irc_message_clear(irc_message_t *message) {
    irc_message_destroy(message);
    irc_message_change(message, NULL, NULL, NULL);
}

/* Channel management */
static void irc_channel_destroy(irc_channel_t *channel) {
    free(channel->channel);
    free(channel->topic);
    hashtable_foreach(channel->users, NULL, &irc_user_destroy);
    hashtable_destroy(channel->users);
    hashtable_foreach(channel->modules, NULL, &irc_module_destroy);
    hashtable_destroy(channel->modules);
    irc_message_destroy(&channel->message);
    free(channel);
}

static void irc_channels_join(irc_t *irc) {
    hashtable_foreach(irc->channels, irc,
        lambda void(irc_channel_t *channel, irc_t *irc)
            => irc_join_raw(irc, channel->channel);
    );
    irc->syncronized = true;
}

module_status_t irc_modules_add(irc_t *irc, const char *name);

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

    irc_message_change(&chan->message, NULL, NULL, NULL);

    /* Deeply copy the config_channel_t modules hashtable and configuration
     * into an irc_channel_t + irc_module_t hashtable.
     */
    hashtable_foreach(channel->modules, chan,
        lambda void(config_module_t *module, irc_channel_t *c) {
            irc_modules_add(c->instance, module->name);
            hashtable_insert(c->modules, module->name, irc_module_create(module));
        }
    );
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

    irc->name         = strdup(instance->name);
    irc->nick         = strdup(instance->nick);
    irc->pattern      = strdup(instance->pattern);
    irc->auth         = (instance->auth) ? strdup(instance->auth) : NULL;
    irc->ready        = false;
    irc->syncronized  = false;
    irc->identified   = false;
    irc->channels     = hashtable_create(64);
    irc->queue        = list_create();
    irc->database     = database_create(instance->database);
    irc->regexprcache = regexpr_cache_create();
    irc->moduleman    = module_manager_create(irc);
    irc->lastunqueue  = 0;

    irc->buffer.data[0]  = '\0';
    irc->buffer.offset   = 0;

    irc_message_change(&irc->message, NULL, NULL, NULL);

    printf("instance: %s\n", irc->name);
    printf("    nick     => %s\n", irc->nick);
    printf("    pattern  => %s\n", irc->pattern);
    printf("    auth     => %s\n", irc->auth ? "(Yes)" : "(No)");
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
    sock_destroy(irc->sock, restart);
    irc_message_destroy(&irc->message);
    free(irc);
}

/* Module management */
module_status_t irc_modules_add(irc_t *irc, const char *name) {
    string_t *error  = NULL;
    module_t *module = NULL;

    if (strstr(name, "//") || strstr(name, "./"))
        return MODULE_STATUS_FAILURE;

    string_t *file = string_format("modules/%s.so", name);
    if ((module = module_manager_search(irc->moduleman, string_contents(file), MMSEARCH_FILE))) {
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

static bool irc_modules_exists(irc_t *irc, const char *name) {
    return list_search(irc->moduleman->modules, name,
        lambda bool(module_t *module, const char *name)
            => return !strcmp(module->name, name);
    );
}

typedef struct {
    size_t      count;
    const char *exclude;
    const char *inmodule;
} irc_module_ref_t;

static size_t irc_modules_refs(irc_t *irc, const char *inmodule, const char *exclude) {
    /* Will exclude `exclude' from the reference search */
    irc_module_ref_t ref = {
        .count    = 0,
        .exclude  = exclude,
        .inmodule = inmodule
    };
    hashtable_foreach(irc->channels, &ref,
        lambda void(irc_channel_t *channel, irc_module_ref_t *ref) {
            if (!strcmp(ref->exclude, channel->channel))
                return;
            hashtable_foreach(channel->modules, ref,
                lambda void(irc_module_t *module, irc_module_ref_t *ref) {
                    if (!strcmp(module->module, ref->inmodule))
                        ref->count++;
                }
            );
        }
    );
    return ref.count;
}
module_status_t irc_modules_reload(irc_t *irc, const char *name) {
    /* Reloading a module reloads it everywhere */
    if (!module_manager_reload(irc->moduleman, name))
        return MODULE_STATUS_FAILURE;
    return MODULE_STATUS_SUCCESS;
}

module_status_t irc_modules_unload(irc_t *irc, const char *channel, const char *module, bool force) {
    size_t refs = irc_modules_refs(irc, module, channel);
    if (refs != 0 && !force)
        return MODULE_STATUS_REFERENCED;
    if (!module_manager_unload(irc->moduleman, module))
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
     * find the appropriate information for it. It's possible that the channel
     * has no module configuration at all.
     */
    list_t            *config   = config_load("config.ini");
    config_instance_t *instance = config_instance_find(config, irc->name);
    config_channel_t  *channel  = config_channel_find(instance, chan);
    if (channel) {
        /* Channel has module configuration */
        config_module_t *module = config_module_find(channel, name);
        hashtable_insert(ch->modules, name, irc_module_create(module));
    } else {
        /* Otherwise there is no configuration for the module */
        config_module_t module = {
            .name = (char *)name,
            .kvs  = hashtable_create(1) /* This is a hack */
        };
        hashtable_insert(ch->modules, name, irc_module_create(&module));
        hashtable_destroy(module.kvs);
    }

    config_unload(config);
    return MODULE_STATUS_SUCCESS;
}

/* The one function gets all loaded modules, while the other shows all the
 * modules on the given channel.
 */
list_t *irc_modules_loaded(irc_t *irc) {
    list_t *list = list_create();

    list_foreach(irc->moduleman->modules, list,
        lambda void(module_t *entry, list_t *list)
            => list_push(list, (char *)entry->name);
    );
    list_sort(list,
        lambda bool(const char *a, const char *b)
            => return strcmp(a, b) >= 0;
    );
    return list;
}

list_t *irc_modules_enabled(irc_t *irc, const char *chan) {
    irc_channel_t *channel = hashtable_find(irc->channels, chan);
    if (!channel)
        return NULL;
    list_t *list = list_create();
    hashtable_foreach(channel->modules, list,
        lambda void(irc_module_t *module, list_t *list)
            => list_push(list, module->module);
    );
    list_sort(list,
        lambda bool(const char *a, const char *b)
            => return strcmp(a, b) >= 0;
    );
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
        irc_channel_t *channel = hashtable_find(irc->channels, params[0]);
        irc_message_t *message = channel ? &channel->message : &irc->message;

        irc_message_update(message, prefix, params[1]);

        /* The bot ignores anyone who is -1 as well as itself */
        if (access_ignore(irc, message->nick) || !strcmp(irc->nick, message->nick))
            return;

        /* Trim trailing whitespace */
        char *trail = message->content + strlen(message->content) - 1;
        while (trail > message->content && isspace(*trail))
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
            module_t *find = module_manager_command(irc->moduleman, skip);
            if (!find) {
                irc_write(irc, message->nick,
                    "Sorry, there is no command named %s available. I do however, take requests if asked nicely.", skip);
                return;
            }

            /* If the channel doesn't have the module we don't bother */
            if (channel && !hashtable_find(channel->modules, skip))
                return;

            /* Skip the initial part of the module */
            char *next = message->content + strlen(irc->pattern) + strlen(skip);
            while (isspace(*next))
                next++;

            cmd_channel_push (
                data,
                cmd_entry_create (
                    data,
                    find,
                    channel ? params[0] : message->nick,
                    message->nick,
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
    } else if (!strncmp(command, "QUIT", end - command)) {
        hashtable_foreach(irc->channels, prefix,
            lambda void(irc_channel_t *channel, const char *nick)
                => irc_users_remove(channel->instance, channel->channel, nick);
        );
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

    for (int i = 0; i < read; ++i) {
        if (temp[i] == '\r')   continue;
        if (temp[i] == '\x02') continue; /* bold      */
        if (temp[i] == '\x1f') continue; /* underline */
        if (temp[i] == '\x16') continue; /* reverse   */
        if (temp[i] == '\x0f') continue; /* reset     */

        /* Dealing with color control:
         *
         *  \x03X
         *  \x03XY
         *  \x03XY,X
         *  \x03XY,XY
         */
        if (temp[i] == '\x03' && ++i < read) {
            /* X or XY skip */
            if (isdigit(temp[i]) && ++i < read && isdigit(temp[i]))
                i++;
            /* ,X or ,XY skip */
            if (i < read && temp[i] == ',') {
                if (++i < read) {
                    if (isdigit(temp[i])) {
                        if (++i < read && isdigit(temp[i]))
                            i++;
                    } else i--; /* Keep comma */
                } else i--; /* Keep comma */
            }
        }

        if (temp[i] == '\n') {
            irc->buffer.data[irc->buffer.offset] = '\0';
            irc_parse(irc, data);
            irc->buffer.offset = 0;
        } else {
            irc->buffer.data[irc->buffer.offset++] = temp[i];
            if (irc->buffer.offset > sizeof(irc->buffer.data) - 1)
                abort();
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

list_t *irc_users(irc_t *irc, const char *channel) {
    irc_channel_t *chan = hashtable_find(irc->channels, channel);
    if (!chan) return NULL;

    list_t *list = list_create();
    hashtable_foreach(chan->users, list,
        lambda void(irc_user_t *user, list_t *list)
            => list_push(list, user->nick);
    );
    list_sort(list,
        lambda bool(const char *a, const char *b)
            => return strcmp(a, b) >= 0;
    );
    return list;
}

list_t *irc_channels(irc_t *irc) {
    list_t *list = list_create();
    hashtable_foreach(irc->channels, list,
        lambda void(irc_channel_t *channel, list_t *list)
            => list_push(list, channel->channel);
    );
    list_sort(list,
        lambda bool(const char *a, const char *b)
            => return strcmp(a, b) >= 0;
    );
    return list;
}
