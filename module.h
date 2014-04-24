#ifndef REDROID_MODULE_HDR
#define REDROID_MODULE_HDR
#include <time.h>
#include <pthread.h>

#include "database.h"
#include "irc.h"
#include "string.h"
#include "mt.h"

typedef struct module_s  module_t;

struct module_s {
    void         *handle;
    const char   *name;
    const char   *match;
    int           interval;
    time_t        lastinterval;
    char         *file;
    void        (*enter)(irc_t *irc, const char *channel, const char *user, const char *message);
    void        (*close)(irc_t *irc);
    irc_t        *instance;
    list_t       *memory;
    mt_t         *random;
};

typedef struct {
    list_t      *modules;
    list_t      *unloaded;
    database_t  *whitelist;
    irc_t       *instance;
} module_manager_t;

/* module */
module_t *module_open(const char *file, module_manager_t *manager, string_t **error);
bool module_reload(module_t *module, module_manager_t *manager);
void module_close(module_t *module, module_manager_t *manager);


/* module manager */
enum {
    MMSEARCH_FILE,
    MMSEARCH_NAME,
    MMSEARCH_MATCH
};

module_manager_t *module_manager_create(irc_t *instance);
void module_manager_destroy(module_manager_t *manager);
bool module_manager_module_unload(module_manager_t *manager, const char *name);
bool module_manager_module_reload(module_manager_t *manager, const char *name);
bool module_manager_module_unloaded(module_manager_t *manager, module_t *module);
module_t *module_manager_module_command(module_manager_t *manager, const char *command);
module_t *module_manager_module_search(module_manager_t *manager, const char *thing, int method);


/* module memory manager */
void module_mem_push(module_t *module, void *data, void (*cleanup)(void *));
void module_mem_destroy(module_t *module);

/* singleton */
void module_singleton_set(module_t *module);
module_t *module_singleton_get(void);
#endif
