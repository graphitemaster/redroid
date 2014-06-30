#ifndef REDROID_MODULEMAN_HDR
#define REDROID_MODULEMAN_HDR
#include "list.h"
#include "irc.h"
#include "database.h"

typedef struct module_manager_s module_manager_t;
typedef struct module_s         module_t;

struct module_manager_s {
    list_t      *modules;
    list_t      *unloaded;
    irc_t       *instance;
};

enum {
    MMSEARCH_FILE,
    MMSEARCH_NAME,
    MMSEARCH_MATCH
};

module_manager_t *module_manager_create(irc_t *instance);
void module_manager_destroy(module_manager_t *manager);
bool module_manager_unload(module_manager_t *manager, const char *name);
bool module_manager_reload(module_manager_t *manager, const char *name);
bool module_manager_unloaded_find(module_manager_t *manager, module_t *module);
void module_manager_unloaded_clear(module_manager_t *manager);
unsigned int module_manager_timeout(module_manager_t *manager);
module_t *module_manager_command(module_manager_t *manager, const char *command);
module_t *module_manager_search(module_manager_t *manager, const char *thing, int method);


#endif
