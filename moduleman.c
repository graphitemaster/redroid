#include <stdlib.h>
#include <string.h>

#include "moduleman.h"

module_manager_t *module_manager_create(irc_t *instance) {
    module_manager_t *manager = malloc(sizeof(*manager));
    manager->instance  = instance;
    manager->modules   = list_create();
    manager->unloaded  = list_create();
    return manager;
}

void module_manager_destroy(module_manager_t *manager) {
    list_foreach(manager->modules, manager, &module_close);
    list_destroy(manager->modules);
    list_destroy(manager->unloaded);
    free(manager);
}

bool module_manager_unload(module_manager_t *manager, const char *name) {
    module_t *find = module_manager_search(manager, name, MMSEARCH_NAME);
    if (!find)
        return false;
    if (!list_erase(manager->modules, find))
        return false;
    return true;
}

bool module_manager_reload(module_manager_t *manager, const char *name) {
    module_t *find = module_manager_search(manager, name, MMSEARCH_NAME);
    if (!find)
        return false;
    if (!module_reload(find, manager))
        return false;
    return true;
}

bool module_manager_unloaded_find(module_manager_t *manager, module_t *module) {
    return list_search(manager->unloaded, module,
        lambda bool(module_t *instance, module_t *module)
            => return instance == module;
    );
}

void module_manager_unloaded_clear(module_manager_t *manager) {
    list_clear(manager->unloaded);
}

module_t *module_manager_command(module_manager_t *manager, const char *command) {
    return module_manager_search(manager, command, MMSEARCH_MATCH);
}

unsigned int module_manager_timeout(module_manager_t *manager) {
    unsigned int timeout = ~0u;
    list_foreach(manager->modules, &timeout,
        lambda void(module_t *module, unsigned int *timeout) {
            if (*module->match == '\0' && module->interval != 0)
                if ((unsigned)module->interval < *timeout)
                    *timeout = module->interval;
        }
    );
    return timeout;
}

typedef struct {
    int         method;
    const char *name;
} module_search_t;

module_t *module_manager_search(module_manager_t *manager, const char *name, int method) {
    return list_search(manager->modules, &((module_search_t){ .method = method, .name = name }),
        lambda bool(module_t *module, module_search_t *search) {
            switch (search->method) {
                case MMSEARCH_FILE:  return !strcmp(search->name, module->file);
                case MMSEARCH_NAME:  return !strcmp(search->name, module->name);
                case MMSEARCH_MATCH: return !strcmp(search->name, module->match);
            }
            return false;
        }
    );
}
