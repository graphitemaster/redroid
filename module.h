#ifndef REDROID_MODULE_HDR
#define REDROID_MODULE_HDR
#include <time.h>

#include "string.h"
#include "irc.h"
#include "mt.h"

typedef struct module_s         module_t;
typedef struct module_manager_s module_manager_t;

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

/* module */
module_t *module_open(const char *file, module_manager_t *manager, string_t **error);
bool module_reload(module_t *module, module_manager_t *manager);
void module_close(module_t *module, module_manager_t *manager);

/* module memory manager */
void module_mem_push(module_t *module, void *data, void (*cleanup)(void *));
void module_mem_destroy(module_t *module);

/* singleton */
void module_singleton_set(module_t *module);
module_t *module_singleton_get(void);
#endif
