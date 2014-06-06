#include "config.h"
#include "ini.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

static config_channel_t *config_channel_create(const char *name) {
    config_channel_t *channel = malloc(sizeof(*channel));
    channel->name    = strdup(name);
    channel->modules = list_create();
    return channel;
}

static void config_channel_destroy(config_channel_t *channel) {
    list_foreach(channel->modules, NULL, &free);
    list_destroy(channel->modules);
    free(channel->name);
    free(channel);
}

static bool config_channel_check(const void *a, const void *b) {
    const char *ca = ((const config_channel_t *)a)->name;
    const char *cb = (const char *)b;

    return !strcmp(ca, cb);
}

static config_channel_t *config_channel_find(list_t *list, const char *name) {
    return list_search(list, &config_channel_check, name);
}

static config_t *config_entry_create(const char *name) {
    config_t *config = malloc(sizeof(*config));
    memset(config, 0, sizeof(*config));
    config->channels = list_create();
    config->name     = strdup(name);
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

    list_foreach(entry->channels, NULL, &config_channel_destroy);
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

static config_channel_t *config_entry_channel_find(list_t *list, const char *name) {
    /*
     * Section name will be "instance:channel". Thus we split the name into two
     * "instance" and "channel". Search for the instance and then search that
     * instances channel list.
     */
    char             *find     = strdup(name);
    char             *split    = strchr(find, ':');
    config_t         *instance = NULL;
    config_channel_t *channel  = NULL;
    if (split) {
        *split = '\0';
        if (!(instance = config_entry_find(list, find)))
            goto config_entry_channel_find_error;
        if (!*++split)
            goto config_entry_channel_find_error;
        if (!(channel = config_channel_find(instance->channels, split)))
            goto config_entry_channel_find_error;

        free(find);
        return channel;
    }

config_entry_channel_find_error:
    free(find);
    return NULL;
}

static bool config_entry_handler(void *user, const char *section, const char *name, const char *value) {
    list_t           *config  = (list_t*)user;
    config_t         *exists  = config_entry_find(config, section);
    config_channel_t *channel = config_entry_channel_find(config, section);

    /* If the entry doesn't exist create it and try again */
    if (!exists && !channel) {
        list_push(config, config_entry_create(section));
        return config_entry_handler(config, section, name, value);
    }

    /*
     * If there is no channel found in the section it means this is a instance
     * section.
     */
    if (channel) {
        /* Per channel options */
        if (!strcmp(name, "modules")) {
            /* Wildcard implies we want all modules */
            if (*value == '*') {
                DIR           *dir;
                struct dirent *ent;
                if ((dir = opendir("modules"))) {
                    while ((ent = readdir(dir))) {
                        if (strstr(ent->d_name, ".so")) {
                            char *copy = strdup(ent->d_name);
                            *strstr(copy, ".so")='\0';
                            list_push(channel->modules, copy);
                        }
                    }
                    closedir(dir);
                } else {
                    fprintf(stderr, "failed to open modules directory\n");
                    return false;
                }
            } else {
                /* Comma separated module list */
                char *tok = strtok((char *)value, ", ");
                while (tok) {
                    char *format = strdup(tok);
                    if (strstr(format, ".so"))
                        *strstr(format, ".so")='\0';
                    list_push(channel->modules, format);
                    tok = strtok(NULL, ", ");
                }
            }
        }
    } else {
        /* Instance options */
        if      (!strcmp(name, "nick"))      exists->nick     = strdup(value);
        else if (!strcmp(name, "pattern"))   exists->pattern  = strdup(value);
        else if (!strcmp(name, "host"))      exists->host     = strdup(value);
        else if (!strcmp(name, "port"))      exists->port     = strdup(value);
        else if (!strcmp(name, "auth"))      exists->auth     = strdup(value);
        else if (!strcmp(name, "database"))  exists->database = strdup(value);
        else if (!strcmp(name, "ssl"))       exists->ssl      = ini_boolean(value);
        else if (!strcmp(name, "channels")) {
            char *tok = strtok((char*)value, ", ");
            while (tok) {
                list_push(exists->channels, config_channel_create(tok));
                tok = strtok(NULL, ", ");
            }
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
    list_foreach(list, NULL, &config_entry_destroy);
    list_destroy(list);
}
