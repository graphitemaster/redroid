#ifndef REDROID_IRC_HDR
#define REDROID_IRC_HDR
#include "sock.h"
#include "list.h"
#include "database.h"
#include "config.h"
#include "regexpr.h"
#include "module.h"

typedef struct irc_s {
    char             *name;         // irc instance name
    char             *nick;         // nick to use on this network
    char             *pattern;      // pattern bot uses to know a command
    char             *auth;         // authenticaion (NickServ)
    sock_t           *sock;         // network socket
    bool              ready;        // ready
    bool              readying;     // readying up
    char              buffer[512];  // processing buffer
    size_t            bufferpos;    // buffer position
    list_t           *channels;     // list of channels for this instance
    list_t           *queue;        // queue of IRC messages
    module_manager_t *moduleman;    // module manager
    database_t       *database;     // database for this IRC instance
    regexpr_cache_t  *regexprcache; // regular expression cache
} irc_t;

irc_t *irc_create(config_t *config);
void  irc_destroy(irc_t *irc);
int irc_process(irc_t *irc, void *data);
const char *irc_name(irc_t *irc);

bool irc_connect(irc_t *irc, const char *host, const char *port, bool ssl);

list_t *irc_modules_list(irc_t *irc);
bool irc_modules_add(irc_t *irc, const char *file);
bool irc_channels_add(irc_t *irc, const char *channel);
int irc_write(irc_t *irc, const char *channel, const char *fmt, ...);
int irc_action(irc_t *irc, const char *channel, const char *fmt, ...);
bool irc_queue_dequeue(irc_t *irc);

#endif
