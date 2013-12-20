#include "irc.h"
#include "sock.h"
#include "list.h"
#include "module.h"

#define _GNU_SOURCE // asprintf
#include <stdlib.h> // malloc, free
#include <string.h> // memset
#include <stdio.h>  // fprintf, stderr
#include <signal.h> // raise, SIGUSR1

struct irc_s {
    char   *name;        // irc instance name
    char   *nick;        // nick to use on this network
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

// Instance management
irc_t *irc_create(const char *name, const char *nick) {
    irc_t *irc = malloc(sizeof(irc_t));

    if (!irc)
        return NULL;

    if (!(irc->name = strdup(name))) {
        free(irc);
        return NULL;
    }

    if (!(irc->nick = strdup(nick))) {
        free(irc->name);
        free(irc);

        return NULL;
    }

    irc->ready     = false;
    irc->readying  = false;
    irc->bufferpos = 0;
    irc->modules   = list_create();
    irc->channels  = list_create();

    printf("instance: %s\n", name);
    return irc;
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
    char     *file   = NULL;

    asprintf(&file, "modules/%s.so", name);

    // prevent loading module twice
    if ((module = irc_modules_find(irc, file))) {
        printf("    module  => %s [%s] already loaded\n", module_name(module), file);
        free(file);
        return false;
    }

    // load the module
    module = module_open(file, irc);
    free(file);

    if (module) {
        list_push(irc->modules, module);
        printf("    module  => %s [%s] loaded\n", module_name(module), module_file(module));
        return true;
    }

    free(file);
    return false;
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

static void irc_process_line(irc_t *irc) {
    const char *line = irc->buffer;
    if (!line || !*line)
        return;

    if (!irc->readying) {
        if (strstr(line, "NOTICE") && !irc->ready) {
            irc->readying = true;
            irc_register(irc);
        }
    } else if (!irc->ready) {
        char ready[128];
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
        // TODO NickServ
    }

    // Just raise internal error if the ircd errors
    if (!strncmp(line, "ERROR :", 7))
        raise(SIGUSR1);

    //
    // TODO:
    // parse where messages come from, and who they go to, channel,
    // user, etc.
    //
    //

    //printf(">> %s\n", line);
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
