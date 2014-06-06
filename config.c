#include "config.h"
#include "ini.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

/*
 * Configuration files take on the format:
 *  - instance(s)
 *      - name
 *      - port
 *      - ssl
 *      - channel(s)
 *          - name
 *          - module(s)
 *              - module name
 *              - module configuration (key value)
 */

/* Modules: (name + config) */
static config_module_t *config_module_create(const char *name) {
    config_module_t *module = malloc(sizeof(*module));
    module->name = strdup(name);
    module->kvs  = hashtable_create(32);
    return module;
}

static void config_module_destroy(config_module_t *module) {
    hashtable_foreach(module->kvs, NULL, &free);
    hashtable_destroy(module->kvs);
    free(module->name);
    free(module);
}

/* Channels: (name + module(s)) */
static config_channel_t *config_channel_create(const char *name) {
    config_channel_t *channel = malloc(sizeof(*channel));
    channel->name    = strdup(name);
    channel->modules = hashtable_create(32);
    return channel;
}

static void config_channel_destroy(config_channel_t *channel) {
    hashtable_foreach(channel->modules, NULL, &config_module_destroy);
    hashtable_destroy(channel->modules);
    free(channel->name);
    free(channel);
}

/* Instance: (name + nick + pattern + host + port + database + auth + channel(s)) */
static config_instance_t *config_instance_create(const char *name) {
    config_instance_t *instance = calloc(1, sizeof(*instance));
    instance->channels = hashtable_create(32);
    instance->name     = strdup(name);
    return instance;
}

static void config_instance_destroy(config_instance_t *instance) {
    free(instance->name);
    free(instance->nick);
    free(instance->pattern);
    free(instance->host);
    free(instance->port);
    free(instance->database);
    free(instance->auth);

    hashtable_foreach(instance->channels, NULL, &config_channel_destroy);
    hashtable_destroy(instance->channels);

    free(instance);
}

static bool config_instance_check(const void *a, const void *b) {
    const char *ca = ((const config_instance_t *)a)->name;
    const char *cb = (const char *)b;

    return !strcmp(ca, cb);
}

/* INI Callback */
static bool config_entry_handler(void *user, const char *section, const char *name, const char *value) {
    list_t *config = (list_t*)user;
    char   *find   = strdup(section);
    char   *split  = strchr(find, ':');

    if (split) {
        *split = '\0';
        char       *modulename  = strchr(split + 1, ':');
        const char *channelname = split + 1;
        if (modulename) {
            *modulename = '\0';
            /* Per module configuration */
            if (!*++modulename) {
                fprintf(stderr, "    config   => [%s] expected module name\n", find);
                goto config_entry_error;
            }
            config_instance_t *instance = config_instance_find(config, find);
            if (!instance) {
                fprintf(stderr, "    config   => failed finding instance `%s'\n", find);
                goto config_entry_error;
            }
            config_channel_t *channel = config_channel_find(instance, channelname);
            if (!channel) {
                fprintf(stderr, "    config   => [%s] failed finding channel `%s'\n", find, channelname);
                goto config_entry_error;
            }
            config_module_t *module = config_module_find(channel, modulename);
            if (!module) {
                fprintf(stderr, "    config   => [%s] failed finding module `%s' for channel `%s'\n", find, modulename, channelname);
                goto config_entry_error;
            }
            hashtable_insert(module->kvs, name, strdup(value));
        } else {
            config_instance_t *instance = config_instance_find(config, find);
            if (!instance) {
                fprintf(stderr, "    config   => failed finding instance `%s'\n", find);
                goto config_entry_error;
            }
            config_channel_t *channel = config_channel_find(instance, channelname);
            if (!channel) {
                fprintf(stderr, "    config   => [%s] failed finding channel `%s'\n", find, channelname);
                goto config_entry_error;
            }
            if (!strcmp(name, "modules")) {
                /* Parse modules for this channel */
                if (*value == '*') {
                    DIR           *dir;
                    struct dirent *ent;
                    if ((dir = opendir("modules"))) {
                        while ((ent = readdir(dir))) {
                            if (strstr(ent->d_name, ".so")) {
                                char *copy = strdup(ent->d_name);
                                *strstr(copy, ".so")='\0';
                                hashtable_insert(channel->modules, copy, config_module_create(copy));
                                free(copy);
                            }
                        }
                        closedir(dir);
                    } else {
                        fprintf(stderr, "   config   => [%s] failed to open `modules' directory\n", find);
                        goto config_entry_error;
                    }
                } else {
                    /* Comma separated module list */
                    char *tok = strtok((char *)value, ", ");
                    while (tok) {
                        char *format = strdup(tok);
                        if (strstr(format, ".so"))
                            *strstr(format, ".so")='\0';
                        hashtable_insert(channel->modules, format, config_module_create(format));
                        tok = strtok(NULL, ", ");
                        free(format);
                    }
                }
            } else {
                fprintf(stderr, "    config   => [%s] unexpected key `%s' for channel `%s' options\n", find, name, channelname);
                goto config_entry_error;
            }
        }
    } else if (!strcmp(section, "web")) {
        /* Web configuration:
         *  TODO.
         */
    } else {
        /* Instance configuration */
        config_instance_t *instance = config_instance_find(config, section);
        if (!instance) {
            /* Create a new instance if we don't already have it and recurse */
            instance = config_instance_create(section);
            list_push(config, instance);
            free(find);
            return config_entry_handler(config, section, name, value);
        } else {
            /* Instance options */
            if      (!strcmp(name, "nick"))      instance->nick     = strdup(value);
            else if (!strcmp(name, "pattern"))   instance->pattern  = strdup(value);
            else if (!strcmp(name, "host"))      instance->host     = strdup(value);
            else if (!strcmp(name, "port"))      instance->port     = strdup(value);
            else if (!strcmp(name, "auth"))      instance->auth     = strdup(value);
            else if (!strcmp(name, "database"))  instance->database = strdup(value);
            else if (!strcmp(name, "ssl"))       instance->ssl      = ini_boolean(value);
            else if (!strcmp(name, "channels")) {
                char *tok = strtok((char*)value, ", ");
                while (tok) {
                    hashtable_insert(instance->channels, tok, config_channel_create(tok));
                    tok = strtok(NULL, ", ");
                }
            }
        }
    }
    free(find);
    return true;

config_entry_error:
    free(find);
    return false;
}

config_channel_t *config_channel_find(config_instance_t *instance, const char *name) {
    return hashtable_find(instance->channels, name);
}

config_module_t *config_module_find(config_channel_t *channel, const char *module) {
    return hashtable_find(channel->modules, module);
}

config_instance_t *config_instance_find(list_t *list, const char *name) {
    return list_search(list, &config_instance_check, name);
}

list_t *config_load(const char *file) {
    list_t *list = list_create();
    if (!ini_parse(file, &config_entry_handler, list))
        return NULL;
    return list;
}

void config_unload(list_t *list) {
    list_foreach(list, NULL, &config_instance_destroy);
    list_destroy(list);
}
