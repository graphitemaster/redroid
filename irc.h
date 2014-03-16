#ifndef REDROID_IRC_HDR
#define REDROID_IRC_HDR
#include "sock.h"
#include "list.h"
#include "database.h"
#include "config.h"
#include "regexpr.h"
#include "module.h"
#include "hashtable.h"

typedef struct irc_manager_s irc_manager_t;

typedef struct {
    char   data[512];
    size_t offset;
} irc_buffer_t;

typedef struct {
    char   *channel;
    char   *topic;
    list_t *users;
} irc_channel_t;

typedef struct {
    char *nick;
    char *name;
    char *host;
    char *channel;
    char *content;
} irc_message_t;

typedef struct irc_s {
    char             *name;
    char             *nick;
    char             *pattern;
    char             *auth;
    sock_t           *sock;
    list_t           *channels;
    module_manager_t *moduleman;
    database_t       *database;
    hashtable_t      *regexprcache;
    irc_manager_t    *manager;
    irc_message_t     message;
    irc_buffer_t      buffer;
    bool              ready;
} irc_t;

irc_t *irc_create(config_t *config);
void  irc_destroy(irc_t *irc, sock_restart_t *restart, char **name);
int irc_process(irc_t *irc, void *data);
const char *irc_name(irc_t *irc);

bool irc_connect(irc_t *irc, const char *host, const char *port, bool ssl);
bool irc_reinstate(irc_t *irc, const char *host, const char *port, sock_restart_t *restart);

list_t *irc_modules_list(irc_t *irc);
list_t *irc_users(irc_t *irc, const char *chan);
bool irc_modules_add(irc_t *irc, const char *file);
bool irc_channels_add(irc_t *irc, const char *channel);
int irc_write(irc_t *irc, const char *channel, const char *fmt, ...);
int irc_action(irc_t *irc, const char *channel, const char *fmt, ...);
bool irc_ready(irc_t *irc);
void irc_unqueue(irc_t *irc);

#endif
