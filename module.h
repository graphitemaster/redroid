#ifndef REDROID_MODULE_HDR
#define REDROID_MODULE_HDR
#include "irc.h"
#include "string.h"

typedef struct module_s          module_t;
typedef struct module_mem_s      module_mem_t;
typedef struct module_mem_node_s module_mem_node_t;

struct module_mem_node_s {
    void              *data;
    void             (*cleanup)(void *);
    module_mem_node_t *next;
};

struct module_s {
    void         *handle;
    const char   *name;
    const char   *match;
    char         *file;
    void        (*enter)(irc_t *irc, const char *channel, const char *user, const char *message);
    void        (*close)(irc_t *irc);
    irc_t        *instance;
    module_mem_t *memory;
};

module_t *module_open(const char *file, irc_t *instance, string_t **error);
void module_destroy(module_t *module);
bool module_reload(module_t *module);
module_t **module_get(void);
void module_mem_mutex_lock(module_t *module);
void module_mem_mutex_unlock(module_t *module);
void module_mem_push(module_t *module, void *data, void (*cleanup)(void *));
module_mem_t *module_mem_create(module_t *instance);
void module_mem_destroy(module_mem_t *mem);


#endif
