#ifndef REDROID_CONFIG_HDR
#define REDROID_CONFIG_HDR
#include "list.h"

typedef struct {
    char   *name;       // network name
    char   *nick;       // nick on this network
    char   *pattern;    // bot pattern
    char   *host;       // server host
    char   *port;       // server port
    char   *auth;       // auth password (NickServ)
    list_t *modules;    // list of modules
    list_t *channels;   // list of channels
} config_t;

list_t *config_load(const char *file);
void config_unload(list_t *list);
#endif
