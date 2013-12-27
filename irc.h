#ifndef REDROID_IRC_HDR
#define REDROID_IRC_HDR
#include "list.h"
#include "database.h"

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

typedef struct irc_s irc_t;

struct irc_s {
    char   *name;        // irc instance name
    char   *nick;        // nick to use on this network
    char   *pattern;     // pattern bot uses to know a command
    char   *auth;        // authenticaion (NickServ)
    int     sock;        // network socket
    bool    ready;       // ready
    bool    readying;    // readying up
    char    buffer[512]; // processing buffer
    size_t  bufferpos;   // buffer position
    list_t *modules;     // list of modules for this instance
    list_t *channels;    // list of channels for this instance
    list_t *queue;       // queue of IRC messages
    database_t *database;
};

irc_t *irc_create(const char *name, const char *nick, const char *auth, const char *pattern, const char *database);
void  irc_destroy(irc_t *irc);
int irc_process(irc_t *irc, void *data);
const char *irc_name(irc_t *irc);

int irc_connect(irc_t *irc, const char *host, const char *port);

bool irc_modules_add(irc_t *irc, const char *file);
bool irc_channels_add(irc_t *irc, const char *channel);


int irc_write(irc_t *irc, const char *channel, const char *fmt, ...);
int irc_action(irc_t *irc, const char *channel, const char *fmt, ...);

#endif
