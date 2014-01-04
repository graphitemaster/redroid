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

static int irc_write_raw(irc_t *irc, const char *channel, const char *message);
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

// Some utility functions
static int irc_pong(irc_t *irc, const char *data) {
    return sock_sendf(irc->sock, "PONG :%s\r\n", data);
}

static int irc_register(irc_t *irc) {
    int accumulate = 0;

    if (irc->ready)
        return -1;

    accumulate = sock_sendf(irc->sock, "NICK %s\r\nUSER %s localhost 0 :redroid\r\n", irc->nick, irc->nick);

    // try auth
    if (irc->auth) {
        // catch early error
        if (accumulate < 0)
            return accumulate;
        accumulate += irc_write(irc, "NickServ", "IDENTIFY %s %s", irc->nick, irc->auth);
    }

    return accumulate;
}

static int irc_quit_raw(irc_t *irc, const char *channel, const char *message) {
    (void)channel;
    return sock_sendf(irc->sock, "QUIT :%s\r\n", message);
}

static int irc_join_raw(irc_t *irc, const char *channel, const char *message) {
    (void)channel;
    return sock_sendf(irc->sock, "JOIN %s\r\n", message);
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

// Instance management
irc_t *irc_create(config_t *entry) {
    irc_t *irc = malloc(sizeof(irc_t));

    if (!irc)
        return NULL;

    if (!(irc->name = strdup(entry->name)))
        goto error;
    if (!(irc->nick = strdup(entry->nick)))
        goto error;
    if (!(irc->pattern = strdup(entry->pattern)))
        goto error;

    if (entry->auth) {
        if (!(irc->auth = strdup(entry->auth)))
            goto error;
    } else {
        irc->auth = NULL;
    }

    irc->ready        = false;
    irc->readying     = false;
    irc->bufferpos    = 0;
    irc->channels     = list_create();
    irc->queue        = list_create();
    irc->database     = database_create(entry->database);
    irc->regexprcache = regexpr_cache_create();
    irc->moduleman    = module_manager_create(irc);

    printf("instance: %s\n", irc->name);
    printf("    nick     => %s\n", irc->nick);
    printf("    pattern  => %s\n", irc->pattern);
    printf("    auth     => %s\n", (irc->auth) ? irc->auth : "(None)");
    printf("    database => %s\n", entry->database);
    printf("    host     => %s\n", entry->host);
    printf("    port     => %s\n", entry->port);

    return irc;

error:
    if (irc->name)     free(irc->name);
    if (irc->nick)     free(irc->nick);
    if (irc->pattern)  free(irc->pattern);
    if (irc->auth)     free(irc->auth);
    if (irc->database) database_destroy(irc->database);
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

    // prevent loading module twice
    if ((module = module_manager_module_search(irc->moduleman, string_contents(file), MMSEARCH_FILE))) {
        printf("    module   => %s [%s] already loaded\n", module->name, name);
        string_destroy(file);
        return false;
    }

    // load the module
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

list_t *irc_modules_list(irc_t *irc) {
    list_t          *list = list_create();
    list_iterator_t *it   = list_iterator_create(irc->moduleman->modules);
    while (!list_iterator_end(it)) {
        module_t *entry = list_iterator_next(it);
        list_push(list, (void *)entry->name);
    }
    list_iterator_destroy(it);
    return list;
}

bool irc_channels_add(irc_t *irc, const char *channel) {
    // prevent adding channel twice
    list_iterator_t *it = list_iterator_create(irc->channels);
    while (!list_iterator_end(it)) {
        if (!strcmp(list_iterator_next(it), channel)) {
            list_iterator_destroy(it);
            printf("    channel  => %s already exists\n", channel);
            return false;
        }
    }
    list_iterator_destroy(it);
    list_push(irc->channels, strdup(channel));
    printf("    channel  => %s added\n", channel);
    return true;
}

void irc_destroy(irc_t *irc) {
    irc_quit_raw(irc, NULL, "Shutting down");

    irc_queue_destroy(irc);

    database_destroy(irc->database);

    regexpr_cache_destroy(irc->regexprcache);

    module_manager_destroy(irc->moduleman);

    // destroy channels
    list_iterator_t *it;
    for (it = list_iterator_create(irc->channels); !list_iterator_end(it); )
        free(list_iterator_next(it));
    list_iterator_destroy(it);
    list_destroy(irc->channels);

    // close the socket
    sock_close(irc->sock);

    free(irc->nick);
    free(irc->name);
    free(irc->pattern);
    free(irc);
}

int irc_connect(irc_t *irc, const char *host, const char *port) {
    if ((irc->sock = sock_get(host, port)) < 0)
        return -1;
    return 0;
}

const char *irc_name(irc_t *irc) {
    return irc->name;
}

static void irc_channels_join(irc_t *irc) {
    list_iterator_t *it = list_iterator_create(irc->channels);
    while (!list_iterator_end(it))
        irc_join_raw(irc, NULL, list_iterator_next(it));
    list_iterator_destroy(it);
}

// trim leading and trailing whitespace from string
static char *irc_process_trim(char *str) {
    char *end;
    while (isspace(*str)) str++;
    if (!*str)
        return str;

    end = str + strlen(str) - 1;
    while (end > str && isspace(*end))
        end--;

    *(end + 1) = '\0';

    return str;
}

static void irc_process_line(irc_t *irc, cmd_channel_t *commander) {
    char *line = irc->buffer;
    if (!line || !*line)
        return;

    //
    // when to know that the IRC server is ready to accept commands from
    // is two stages:
    //  stage 1: readying up: i.e first NOTICE from the server
    //  stage 2: waiting for the server to accept the ident and get 001 NICK
    //
    //  after these stages succeed we're ready to send other commands, such
    //  as QUIT and JOIN
    //
    if (!irc->readying) {
        if (strstr(line, "NOTICE") && !irc->ready) {
            irc->readying = true;
            irc_register(irc);
        }
    } else if (!irc->ready) {
        char ready[512];
        snprintf(ready, sizeof(ready), "001 %s", irc->nick);
        if (strstr(line, ready)) {
            irc->ready = true;
            irc_channels_join(irc);
        } else {
            return;
        }
    }

    if (!strncmp(line, "PING :", 6))
        irc_pong(irc, line + 6);

    if (!strncmp(line, "NOTICE AUTH :", 13)) {
        // ignore
    }

    char *nick    = NULL;
    char *message = NULL;
    char *channel = NULL;
    bool  private = false;
    bool  kick    = false;

    // Just raise internal error if the ircd errors
    if (!strncmp(line, "ERROR :", 7))
        raise(SIGUSR1);

    // non message strings are ignored
    if (strchr(line, 1))
        return;

    // parse the contents of the line
    if (*line == ':') {
        char *ptr = strtok(line + 1, "!");
        if (!ptr)
            return;

        nick = strdup(ptr);

        while ((ptr = strtok(NULL, " "))) {
            if (!strcmp(ptr, "PRIVMSG")) {
                private = true;
                break;
            } else if (!strcmp(ptr, "KICK")) {
                kick = true;
                break;
            }
        }

        if (private || kick) {
            if ((ptr = strtok(NULL, ":")))
                channel = strdup(ptr);
            if ((ptr = strtok(NULL, "")))
                message = strdup(ptr);
        }

        if (kick && channel) {
            irc_join_raw(irc, NULL, channel);

            free(channel);

            if (nick)
                free(nick);
            if (message)
                free(message);

            return;
        }

        if (channel) {
            if (private && strlen(nick) > 0) {
                if (strlen(message) > 0 && !strncmp(message, irc->pattern, strlen(irc->pattern))) {
                    // get the command entry and call it
                    char *copy  = strdup(message + strlen(irc->pattern));
                    char *match = strchr(copy, ' ');
                    if (match)
                        *match = '\0';

                    module_t *module = module_manager_module_command(irc->moduleman, irc_process_trim(copy));

                    if (module) {
                        cmd_channel_push(
                            commander,
                            cmd_entry_create(
                                commander,
                                module,
                                channel,
                                nick,
                                irc_process_trim(message + strlen(irc->pattern) + strlen(copy))
                            )
                        );
                    } else {
                        // make it go to PRIVMSG
                        irc_write(irc, nick, "Sorry, there is no command named %s available. I do however, take requests if asked nicely.", copy);
                    }
                    free(copy);
                }

                // run all modules with no match rule
                list_iterator_t *it = list_iterator_create(irc->moduleman->modules);
                while (!list_iterator_end(it)) {
                    module_t *module = list_iterator_next(it);
                    if (!strlen(module->match))
                        cmd_channel_push(commander, cmd_entry_create(commander, module, channel, nick, message));
                }
                list_iterator_destroy(it);
            }

            free(channel);
        }

        if (nick)
            free(nick);

        if (message)
            free(message);
    }
}

int irc_process(irc_t *irc, void *data) {
    cmd_channel_t *commander = data;

    char temp[128];
    int  read;

    if ((read = sock_recv(irc->sock, temp, sizeof(temp) - 2)) <= 0)
        return -1;

    temp[read] = '\0';

    for (size_t i = 0; i < read; ++i) {
        switch (temp[i]) {
            case '\r':
            case '\n':
                irc->buffer[irc->bufferpos] = '\0';
                irc->bufferpos              = 0;

                irc_process_line(irc, commander);
                break;

            // ignore stuff
            case '\x13':    // strike-through
            case '\x15':    // reset
            case '\x1F':    // underline
            case '\x16':    // reverse
            case '\x00':    // white
            case '\x01':    // black
            case '\x02':    // dark blue  / (control: bold)
            case '\x03':    // dark green / (control: color)
            case '\x04':    // red
            case '\x05':    // dark red
            case '\x06':    // dark violet
            case '\x07':    // orange
            case '\x08':    // yellow
            case '\x09':    // light green  / (control: italic)
            //case '\x0A':    // cornflower blue
            case '\x0B':    // light blue
            case '\x0C':    // blue
            //case '\x0D':    // violet
            case '\x0E':    // dark grey
            case '\x0F':    // light grey
                break;

            default:
                irc->buffer[irc->bufferpos] = temp[i];
                if (irc->bufferpos >= sizeof(irc->buffer) - 1)
                    ;
                else
                    irc->bufferpos++;
        }
    }

    return 0;
}
