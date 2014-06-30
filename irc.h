#ifndef REDROID_IRC_HDR
#define REDROID_IRC_HDR
#include <time.h>
#include <stdarg.h>

#include "sock.h"
#include "list.h"
#include "database.h"
#include "config.h"
#include "regexpr.h"
#include "module.h"
#include "moduleman.h"
#include "hashtable.h"

#define RPL_WELCOME        1
#define RPL_TOPIC          332
#define RPL_NAMREPLY       353
#define RPL_MOTD           372
#define RPL_ENDOFMOTD      376

#define ERR_NOMOTD         422
#define ERR_NICKNAMEINUSE  433

#define IRC_FLOOD_LINES    4 /* lines per IRC_FLOOD_INTERVAL */
#define IRC_FLOOD_INTERVAL 1 /* seconds */

typedef struct irc_manager_s irc_manager_t;

typedef struct {
    char   data[513];
    size_t offset;
} irc_buffer_t;

typedef struct {
    char *nick;
    char *host;
} irc_user_t;

typedef struct {
    char        *module;
    hashtable_t *kvs; /* map<char *> */
} irc_module_t;

typedef struct {
    char *nick;
    char *host;
    char *content;
} irc_message_t;

typedef struct {
    char          *channel;
    char          *topic;
    hashtable_t   *users;   /* map<irc_user_t>   */
    hashtable_t   *modules; /* map<irc_module_t> */
    irc_t         *instance;
    irc_message_t  message;
} irc_channel_t;

struct irc_s {
    char             *name;
    char             *nick;
    char             *pattern;
    char             *auth;
    sock_t           *sock;
    hashtable_t      *channels;     /* map<irc_channel_t> */
    list_t           *queue;
    module_manager_t *moduleman;
    database_t       *database;
    regexpr_cache_t  *regexprcache;
    irc_manager_t    *manager;
    irc_buffer_t      buffer;
    irc_message_t     message;
    bool              ready;
    bool              syncronized;
    bool              identified;
    time_t            lastunqueue;
};

typedef enum {
    MODULE_STATUS_REFERENCED,
    MODULE_STATUS_SUCCESS,
    MODULE_STATUS_FAILURE,
    MODULE_STATUS_ALREADY,
    MODULE_STATUS_NONEXIST
} module_status_t;

typedef struct irc_s irc_t;

irc_t *irc_create(config_instance_t *config);
void  irc_destroy(irc_t *irc, sock_restart_t *restart, char **name);
void irc_process(irc_t *irc, void *data);
const char *irc_name(irc_t *irc);
const char *irc_nick(irc_t *irc);

bool irc_connect(irc_t *irc, const char *host, const char *port, bool ssl);
bool irc_reinstate(irc_t *irc, const char *host, const char *port, sock_restart_t *restart);

list_t *irc_users(irc_t *irc, const char *chan);
list_t *irc_channels(irc_t *irc);

list_t *irc_modules_loaded(irc_t *irc);
list_t *irc_modules_enabled(irc_t *irc, const char *channel);
module_status_t irc_modules_add(irc_t *irc, const char *name);
module_status_t irc_modules_reload(irc_t *irc, const char *name);
module_status_t irc_modules_unload(irc_t *irc, const char *channel, const char *module, bool force);
module_status_t irc_modules_disable(irc_t *irc, const char *chan, const char *name);
module_status_t irc_modules_enable(irc_t *irc, const char *chan, const char *name);

bool irc_channels_add(irc_t *irc, config_channel_t *channel);

void irc_message_clear(irc_message_t *message);

void irc_writev(irc_t *irc, const char *channel, const char *fmt, va_list);
void irc_actionv(irc_t *irc, const char *channel, const char *fmt, va_list);
void irc_write(irc_t *irc, const char *channel, const char *fmt, ...);
void irc_action(irc_t *irc, const char *channel, const char *fmt, ...);
void irc_unqueue(irc_t *irc);
void irc_join(irc_t *irc, const char *channel);
void irc_part(irc_t *irc, const char *channel);
const char *irc_topic(irc_t *irc, const char *channel);
const char *irc_pattern(irc_t *irc, const char *newpattern);

#endif
