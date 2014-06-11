#include "config.h"
#include "string.h"
#include "list.h"
#include "ini.h"

#include <time.h>
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
    channel->name       = strdup(name);
    channel->modules    = hashtable_create(32);
    channel->modulesall = false;
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
            config_channel_t  *channel  = config_channel_create(channelname);

            hashtable_insert(instance->channels, channelname, channel);

            if (!strcmp(name, "modules")) {
                /* Parse modules for this channel */
                channel->modulesall = true;
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
    return list_search(list, name,
        lambda bool(const config_instance_t *instance, const char *name) {
            return !strcmp(instance->name, name);
        }
    );
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

typedef struct {
    FILE              *fp;
    config_instance_t *instance;
    config_channel_t  *channel;
    string_t          *modsline;
} config_save_t;

void config_save(list_t *config, const char *file) {
    FILE *fp;
    if (!(fp = fopen(file, "w")))
        return;

    char timestamp[256];
    strftime(timestamp, sizeof(timestamp) - 1,
        "%B %d, %Y %I:%M %p", localtime(&(time_t){time(0)}));
    fprintf(fp, "# Redroid configuration last modified %s\n", timestamp);

    /* For all instances */
    list_foreach(config, fp,
        lambda void(config_instance_t *instance, FILE *fp) {
            config_save_t save = {
                .fp       = fp,
                .instance = instance
            };

            /*
             * Note: There is 8 spaces to `=' if this changes, please change the
             * right justification `%-8s' for the module configuration alignment
             * below.
             */
            fprintf(fp, "[%s]\n",              instance->name);
            fprintf(fp, "    nick     = %s\n", instance->nick);
            fprintf(fp, "    pattern  = %s\n", instance->pattern);
            fprintf(fp, "    host     = %s\n", instance->host);
            fprintf(fp, "    port     = %s\n", instance->port);
            fprintf(fp, "    auth     = %s\n", instance->auth);
            fprintf(fp, "    database = %s\n", instance->database);
            fprintf(fp, "    ssl      = %s\n\n", instance->ssl ? "True" : "False");

            fprintf(fp, "# Channels for `%s'\n", instance->name);
            hashtable_foreach(instance->channels, &save,
                lambda void(config_channel_t *channel, config_save_t *save) {
                    save->channel = channel;
                    fprintf(save->fp, "[%s:%s]\n", save->instance->name, save->channel->name);
                    fprintf(save->fp, "    modules  = ");
                    if (save->channel->modulesall)
                        fprintf(save->fp, "*\n\n");
                    else {
                        save->modsline = string_construct();
                        hashtable_foreach(save->channel->modules, save,
                            lambda void(config_module_t *module, config_save_t *save) {
                                string_catf(save->modsline, "%s, ", module->name);
                            }
                        );
                        string_shrink(save->modsline, 2);
                        fprintf(save->fp, "%s\n\n", string_contents(save->modsline));
                        string_destroy(save->modsline);
                    }

                    fprintf(save->fp, "# Module configuration for `%s'\n\n",
                        save->channel->name);

                    hashtable_foreach(save->channel->modules, save,
                        lambda void(config_module_t *module, config_save_t *save) {
                            /* Don't bother writing module config unless we need to */
                            if (hashtable_elements(module->kvs) == 0)
                                return;
                            fprintf(save->fp, "# Module configuration for `%s'\n", module->name);
                            fprintf(save->fp, "[%s:%s:%s]\n",
                                save->instance->name, save->channel->name, module->name);
                            hashtable_foreachkv(module->kvs, save->fp,
                                lambda void(const char *key, const char *value, FILE *fp) {
                                    fprintf(fp, "    %-8s = %s\n", key, value);
                                }
                            );
                        }
                    );
                }
            );
        }
    );
    fclose(fp);
}
