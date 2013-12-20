#include "config.h"
#include "ini.h"

#include <string.h> // strlen, memset
#include <stdlib.h> // malloc, free, strdup

static config_t *config_entry_create(void) {
    config_t *config = malloc(sizeof(*config));
    memset(config, 0, sizeof(*config));
    config->modules  = list_create();
    config->channels = list_create();

    return config;
}

static void config_entry_destroy(config_t *entry) {
    free(entry->name);
    free(entry->nick);
    free(entry->pattern);
    free(entry->host);
    free(entry->port);

    list_iterator_t *it;
    for (it = list_iterator_create(entry->modules); !list_iterator_end(it); )
        free(list_iterator_next(it));
    list_iterator_destroy(it);

    for (it = list_iterator_create(entry->channels); !list_iterator_end(it); )
        free(list_iterator_next(it));
    list_iterator_destroy(it);

    list_destroy(entry->modules);
    list_destroy(entry->channels);

    free(entry);
}

static config_t *config_entry_find(list_t *list, const char *name) {
    config_t        *entry = NULL;
    list_iterator_t *it    = list_iterator_create(list);
    for (; !list_iterator_end(it); ) {
        entry = list_iterator_next(it);
        if (!strcmp(entry->name, name))
            break;
    }
    list_iterator_destroy(it);
    return entry;
}

static bool config_entry_handler(void *user, const char *section, const char *name, const char *value) {
    list_t   *config = (list_t*)user;
    config_t *exists = config_entry_find(config, section);

    if (!exists) {
        exists       = config_entry_create();
        exists->name = strdup(section);

        list_push(config, exists);
    }

    if      (!strcmp(name, "nick"))    exists->nick    = strdup(value);
    else if (!strcmp(name, "pattern")) exists->pattern = strdup(value);
    else if (!strcmp(name, "host"))    exists->host    = strdup(value);
    else if (!strcmp(name, "port"))    exists->port    = strdup(value);
    else if (!strcmp(name, "modules")) {
        char *tok = strtok((char *)value, ", ");
        while (tok) {
            list_push(exists->modules, strdup(tok));
            tok = strtok(NULL, ", ");
        }
    }
    else if (!strcmp(name, "channels")) {
        char *tok = strtok((char*)value, ", ");
        while (tok) {
            list_push(exists->channels, strdup(tok));
            tok = strtok(NULL, ", ");
        }
    }

    return true;
}

list_t *config_load(const char *file) {
    list_t *list = list_create();
    if (!ini_parse(file, &config_entry_handler, list))
        return NULL;
    return list;
}

void config_unload(list_t *list) {
    list_iterator_t *it;
    for (it = list_iterator_create(list); !list_iterator_end(it); )
        config_entry_destroy(list_iterator_next(it));
    list_iterator_destroy(it);
    list_destroy(list);
}
