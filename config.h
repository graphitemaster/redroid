#ifndef REDROID_CONFIG_HDR
#define REDROID_CONFIG_HDR
#include "list.h"
#include "hashtable.h"

typedef struct {
    hashtable_t *kvs;        /* map<char *> (key value store)  */
    char        *name;       /* module name                    */
} config_module_t;

typedef struct {
    hashtable_t *modules;    /* map<config_module_t>           */
    char        *name;       /* channel name                   */
} config_channel_t;

typedef struct {
    char        *name;       /* network name                   */
    char        *nick;       /* nick on this network           */
    char        *pattern;    /* bot pattern                    */
    char        *host;       /* server host                    */
    char        *port;       /* server port                    */
    char        *auth;       /* auth password (NickServ)       */
    char        *database;   /* database file for IRC instance */
    bool         ssl;        /* SSL network                    */
    hashtable_t *channels;   /* map<config_channel_t>          */
} config_instance_t;

config_channel_t *config_channel_find(config_instance_t *instance, const char *name);
config_module_t *config_module_find(config_channel_t *channel, const char *module);
config_instance_t *config_instance_find(list_t *list, const char *name);

list_t *config_load(const char *file);
void config_unload(list_t *list);
#endif
