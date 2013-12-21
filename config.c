#include "config.h"
#include "ini.h"

#include <stdio.h>  // asprintf
#include <string.h> // strlen, memset, strstr
#include <stdlib.h> // malloc, free, strdup
#include <dirent.h> // DIR, dirent, readdir, closedir

#define MODULE_DIR "modules"

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

    if      (!strcmp(name, "nick"))      exists->nick    = strdup(value);
    else if (!strcmp(name, "pattern"))   exists->pattern = strdup(value);
    else if (!strcmp(name, "host"))      exists->host    = strdup(value);
    else if (!strcmp(name, "port"))      exists->port    = strdup(value);
    else if (!strcmp(name, "modules")) {
        if (*value == '*') {
            // load all modules
            DIR           *dir;
            struct dirent *ent;
            if ((dir = opendir(MODULE_DIR))) {
                while ((ent = readdir(dir))) {
                    if (strstr(ent->d_name, ".so")) { // found module
                        char *format = NULL;
                        asprintf(&format, "%s/%s", MODULE_DIR, ent->d_name);
                        list_push(exists->modules, format);
                    }
                }
                closedir(dir);
            } else {
                fprintf(stderr, "failed to open modules directory: %s\n", MODULE_DIR);
                return false;
            }
        } else {
            // individually
            char *tok = strtok((char *)value, ", ");
            while (tok) {
                char *format = NULL;
                if (!strstr(tok, ".so")) // no .so add it
                    asprintf(&format, "%s/%s.so", MODULE_DIR, tok);
                else
                    asprintf(&format, "%s/%s", MODULE_DIR, tok);

                list_push(exists->modules, format);
                tok = strtok(NULL, ", ");
            }
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
