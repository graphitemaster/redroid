#ifndef REDROID_IRC_HDR
#define REDROID_IRC_HDR
#include "sock.h"
#include "list.h"
#include "database.h"
#include "config.h"
#include "regexpr.h"
#include "module.h"
#include "ircparser.h"

typedef struct irc_manager_s irc_manager_t;

typedef enum {
    IRC_COMMAND_PING     = 1 << 1,  /* Set when pinged then unset when ponged        */
    IRC_COMMAND_ERROR    = 1 << 2,  /* Set when error then unsed when handled error  */
    IRC_COMMAND_KICK     = 1 << 3,  /* Set when kicked from channel unset on rejoin  */
    IRC_COMMAND_JOIN     = 1 << 4,  /* Set when user enters channel                  */
    IRC_COMMAND_LEAVE    = 1 << 5,  /* Set when user leaves channel                  */
    IRC_COMMAND_NOTICE   = 1 << 6,  /* Set when server notice (ircd-seven crap)      */
    IRC_COMMAND_AUTH     = 1 << 7,  /* Set when server auth (sane ircds)             */
    IRC_COMMAND_PRIVMSG  = 1 << 8,  /* Set when server privmsg                       */

    IRC_STATE_AUTH       = 1 << 9,  /* Set when authenticated (registered)           */
    IRC_STATE_NICKSERV   = 1 << 10, /* Set when auth'd with NickServ if there is any */
    IRC_STATE_READY      = 1 << 11, /* Set when ready to send commands to the server */
    IRC_STATE_END        = 1 << 12, /* Set when reached EOL from server              */

    /*
     * We consider a single NOTICE or a single AUTH command as a singular
     * register command.
     */
    IRC_COMMAND_REGISTER = (IRC_COMMAND_AUTH | IRC_COMMAND_NOTICE)
} irc_flags_t;

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
    list_t           *queue;
    module_manager_t *moduleman;
    database_t       *database;
    regexpr_cache_t  *regexprcache;
    irc_manager_t    *manager;
    irc_parser_t      parser;
    irc_message_t     message;
    irc_flags_t       flags;
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
bool irc_queue_dequeue(irc_t *irc);

#endif
