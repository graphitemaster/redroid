#include "irc.h"
#include "sock.h"
#include "list.h"
#include "module.h"

#define _GNU_SOURCE // asprintf
#include <stdlib.h> // malloc, free
#include <string.h> // memset
#include <stdio.h>  // fprintf, stderr
#include <signal.h> // raise, SIGUSR1
#include <stdarg.h> // va_list, va_start, va_end
#include <ctype.h>  // isspace

struct irc_s {
    char   *name;        // irc instance name
    char   *nick;        // nick to use on this network
    char   *pattern;     // pattern bot uses to know a command
    int     sock;        // network socket
    bool    ready;       // ready
    bool    readying;    // readying up
    char    buffer[512]; // processing buffer
    size_t  bufferpos;   // buffer position
    list_t *modules;     // list of modules for this instance
    list_t *channels;    // list of channels for this instance
};

// Some utility functions
static int irc_pong(irc_t *irc, const char *data) {
    return sock_sendf(irc->sock, "PONG :%s\r\n", data);
}
static int irc_register(irc_t *irc) {
    if (irc->ready)
        return -1;
    return sock_sendf(irc->sock, "NICK %s\r\nUSER %s localhost 0 :redroid\r\n", irc->nick, irc->nick);
}
static int irc_nick(irc_t *irc, const char *nick) {
    free(irc->nick);
    irc->nick = strdup(nick);
    return sock_sendf(irc->sock, "NICK %s\r\n", nick);
}
static int irc_quit(irc_t *irc, const char *message) {
    return sock_sendf(irc->sock, "QUIT :%s\r\n", message);
}
static int irc_message(irc_t *irc, const char *channel, const char *data) {
    return sock_sendf(irc->sock, "PRIVMSG %s :%s\r\n", channel, data);
}
static int irc_join(irc_t *irc, const char *channel) {
    return sock_sendf(irc->sock, "JOIN %s\r\n", channel);
}

int irc_action(irc_t *irc, const char *channel, const char *fmt, ...) {
    char *buffer = NULL;
    va_list va;
    va_start(va, fmt);
    vasprintf(&buffer, fmt, va);
    va_end(va);
    int ret = sock_sendf(irc->sock, "PRIVMSG %s :\001ACTION %s\001\r\n", channel, buffer);
    free(buffer);
    return ret;
}

int irc_write(irc_t *irc, const char *channel, const char *fmt, ...) {
    char *buffer = NULL;
    va_list  va;
    va_start(va, fmt);
    vasprintf(&buffer, fmt, va);
    va_end(va);
    int ret = irc_message(irc, channel, buffer);
    free(buffer);
    return ret;
}

// Instance management
irc_t *irc_create(const char *name, const char *nick, const char *pattern) {
    irc_t *irc = malloc(sizeof(irc_t));

    if (!irc)
        return NULL;

    if (!(irc->name = strdup(name)))
        goto error;
    if (!(irc->nick = strdup(nick)))
        goto error;
    if (!(irc->pattern = strdup(pattern)))
        goto error;

    irc->ready     = false;
    irc->readying  = false;
    irc->bufferpos = 0;
    irc->modules   = list_create();
    irc->channels  = list_create();

    printf("instance: %s\n", name);
    return irc;

error:
    if (irc->name)    free(irc->name);
    if (irc->nick)    free(irc->nick);
    if (irc->pattern) free(irc->pattern);
    return NULL;
}

module_t *irc_modules_find(irc_t *irc, const char *file) {
    list_iterator_t *it;
    for (it = list_iterator_create(irc->modules); !list_iterator_end(it); ) {
        module_t *module = list_iterator_next(it);
        if (!strcmp(module_file(module), file)) {
            list_iterator_destroy(it);
            return module;
        }
    }
    list_iterator_destroy(it);
    return NULL;
}

bool irc_modules_add(irc_t *irc, const char *name) {
    module_t *module = NULL;

    // prevent loading module twice
    if ((module = irc_modules_find(irc, name))) {
        printf("    module  => %s [%s] already loaded\n", module_name(module), name);
        return false;
    }

    // load the module
    module = module_open(name, irc);

    if (module) {
        list_push(irc->modules, module);
        printf("    module  => %s [%s] loaded\n", module_name(module), module_file(module));
        return true;
    }
    printf("    module  => %s loading failed\n", name);
    return false;
}

void (*irc_modules_command(irc_t *irc, const char *command))(irc_t *, const char *, const char *, const char *) {
    list_iterator_t *it = list_iterator_create(irc->modules);
    while (!list_iterator_end(it)) {
        module_t *entry = list_iterator_next(it);
        if (!strcmp(entry->match, command)) {
            list_iterator_destroy(it);
            return entry->enter;
        }
    }
    list_iterator_destroy(it);
    return NULL;
}

bool irc_channels_add(irc_t *irc, const char *channel) {
    // prevent adding channel twice
    list_iterator_t *it = list_iterator_create(irc->channels);
    while (!list_iterator_end(it)) {
        if (!strcmp(list_iterator_next(it), channel)) {
            list_iterator_destroy(it);
            printf("    channel => %s already exists\n", channel);
            return false;
        }
    }
    list_iterator_destroy(it);
    list_push(irc->channels, strdup(channel));
    printf("    channel => %s added\n", channel);
    return true;
}

void irc_destroy(irc_t *irc) {
    irc_quit(irc, "Shutting down");

    // destory modules
    list_iterator_t *it;
    for (it = list_iterator_create(irc->modules); !list_iterator_end(it); )
        module_close(list_iterator_next(it));
    list_iterator_destroy(it);
    list_destroy(irc->modules);

    // destroy channels
    for (it = list_iterator_create(irc->channels); !list_iterator_end(it); )
        free(list_iterator_next(it));
    list_iterator_destroy(it);
    list_destroy(irc->channels);

    // close the socket
    sock_close(irc->sock);

    // free other data
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
        irc_join(irc, list_iterator_next(it));
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

    *(end+1)='\0';

    return str;
}

static void irc_process_line(irc_t *irc) {
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

    //
    // Deal with NickServ on networks that support it:
    //  TODO
    //
    if (!strncmp(line, "NOTICE AUTH :", 13)) {

    }

    char *nick    = NULL;
    char *message = NULL;
    char *channel = NULL;
    bool  private = false;

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
            }
        }

        if (private) {
            if ((ptr = strtok(NULL, ":")))
                channel = strdup(ptr);
            if ((ptr = strtok(NULL, "")))
                message = strdup(ptr);
        }

        // write it back out to the channel
        if (channel) {
            void (*cmd)(irc_t *, const char *, const char *, const char *) = NULL;
            if (private && strlen(nick) > 0 && strlen(message) > 0) {
                if (!strncmp(message, irc->pattern, strlen(irc->pattern))) {
                    // get the command entry and call it
                    char *copy  = strdup(message + strlen(irc->pattern));
                    char *match = strchr(copy, ' ');
                    if (match)
                        *match = '\0';

                    if ((cmd = irc_modules_command(irc, copy))) {
                        // trim trailing white
                        char *trim = irc_process_trim(message + strlen(irc->pattern) + strlen(copy) + 1);
                        cmd(irc, channel, nick, trim); // "stuff"
                    } else {
                        char *trim = irc_process_trim(message + strlen(irc->pattern));
                        irc_write(irc, channel, "unknown command %s%s", irc->pattern, trim);
                    }

                    free(copy);
                }
            }
            free(channel);
        }

        if (nick)    free(nick);
        if (message) free(message);
    }
}

int irc_process(irc_t *irc) {
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

                irc_process_line(irc);
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
