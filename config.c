#include "config.h"
#include "ini.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

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
    free(entry->database);
    free(entry->auth);

    list_foreach(entry->modules, &free);
    list_foreach(entry->channels, &free);
    list_destroy(entry->modules);
    list_destroy(entry->channels);

    free(entry);
}

static bool config_entry_check(const void *element, const void *passed) {
    const config_t *entry = element;
    if (!strcmp(entry->name, (const char *)passed))
        return true;
    return false;
}

static config_t *config_entry_find(list_t *list, const char *name) {
    return list_search(list, &config_entry_check, name);
}

static bool config_entry_handler(void *user, const char *section, const char *name, const char *value) {
    list_t   *config = (list_t*)user;
    config_t *exists = config_entry_find(config, section);

    if (!exists) {
        exists       = config_entry_create();
        exists->name = strdup(section);

        list_push(config, exists);
        return config_entry_handler(config, section, name, value);
    }

    if      (!strcmp(name, "nick"))      exists->nick     = strdup(value);
    else if (!strcmp(name, "pattern"))   exists->pattern  = strdup(value);
    else if (!strcmp(name, "host"))      exists->host     = strdup(value);
    else if (!strcmp(name, "port"))      exists->port     = strdup(value);
    else if (!strcmp(name, "auth"))      exists->auth     = strdup(value);
    else if (!strcmp(name, "database"))  exists->database = strdup(value);
    else if (!strcmp(name, "ssl"))       exists->ssl      = ini_boolean(value);
    else if (!strcmp(name, "modules")) {
        if (*value == '*') {
            /* Load all modules */
            DIR           *dir;
            struct dirent *ent;
            if ((dir = opendir("modules"))) {
                while ((ent = readdir(dir))) {
                    if (strstr(ent->d_name, ".so")) {
                        /* Found module */
                        char *copy = strdup(ent->d_name);
                        *strstr(copy, ".so")='\0';
                        list_push(exists->modules, copy);
                    }
                }
                closedir(dir);
            } else {
                fprintf(stderr, "failed to open modules directory\n");
                return false;
            }
        } else {
            /* Individual modules */
            char *tok = strtok((char *)value, ", ");
            while (tok) {
                char *format = strdup(tok);
                if (strstr(format, ".so"))
                    *strstr(format, ".so")='\0';
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
    list_foreach(list, (void(*)(void*))&config_entry_destroy);
    list_destroy(list);
}
