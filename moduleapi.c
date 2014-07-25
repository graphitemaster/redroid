/* Implementation of the Redroid API for modules */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "module.h"
#include "access.h"
#include "irc.h"

void redroid_restart(irc_t *irc, const char *channel, const char *user);
void redroid_shutdown(irc_t *irc, const char *channel, const char *user);
void redroid_recompile(irc_t *irc, const char *channel, const char *user);
void redroid_daemonize(irc_t *irc, const char *channel, const char *user);
const char *redroid_buildinfo(void);

/* api runtime */
static hashtable_t *module_api_irc_modules_config_copy(irc_t *irc, const char *mname, const char *cname) {
    irc_channel_t *channel = hashtable_find(irc->channels, cname);
    if (!channel)
        return NULL;
    irc_module_t *module = hashtable_find(channel->modules, mname);
    if (!module)
        return NULL;
    return hashtable_copy(module->kvs, &strdup);
}

static void module_api_irc_modules_config_destroy(hashtable_t *kvs) {
    hashtable_foreach(kvs, &free);
    hashtable_destroy(kvs);
}

static list_t* module_api_strnsplit_impl(char *str, const char *delim, size_t count) {
    list_t *list = list_create();
    char *saveptr;
    char *end = str + strlen(str);
    char *tok = strtok_r(str, delim, &saveptr);
    while (tok) {
        list_push(list, tok);
        if (!--count) {
            tok += strlen(tok);
            if (tok == end)
                return list;
            for (++tok; *tok && strchr(delim, *tok);)
                ++tok;
            if (*tok)
                list_push(list, tok);
            return list;
        }
        tok = strtok_r(NULL, delim, &saveptr);
    }
    return list;
}

typedef struct {
    string_t *message;
    string_t *author;
    string_t *revision;
} svn_entry_t;

/* a simple way to parse the svn log */
static svn_entry_t *module_api_svnlog_read_entry(FILE *handle) {
    svn_entry_t *entry  = malloc(sizeof(*entry));
    char        *line   = NULL;
    size_t       length = 0;

    if (getline(&line, &length, handle) == EOF) {
        free(entry);
        free(line);
        return NULL;
    }

    /* if we have '-' at start then it's a seperator */
    if (*line == '-') {
        if (getline(&line, &length, handle) == EOF) {
            free(entry);
            free(line);
            return NULL;
        }
    }

    /* expecting a revision tag */
    if (*line != 'r') {
        free(entry);
        free(line);
        return NULL;
    }

    /* split revision marker "rev | author | date | changes" */
    char   *copy  = strdup(&line[1]);
    list_t *split = module_api_strnsplit_impl(copy, " |", 2);
    list_pop(split); /* drop "date | changes" */

    entry->revision = string_create(list_shift(split));
    entry->author   = string_create(list_shift(split));

    /* now parse it */
    string_t *message = string_construct();
    while (getline(&line, &length, handle) != EOF) {
        /* ignore empty lines */
        if (*line == '\n')
            continue;

        /* seperator marks end of message */
        if (*line == '-') {
            entry->message = message;
            list_destroy(split);
            free(copy);
            free(line);
            return entry;
        }

        /* strip newlines */
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';

        string_catf(message, "%s", line);
    }

    string_destroy(message);
    list_destroy(split);
    free(copy);
    free(entry);
    free(line);

    return NULL;
}

static void module_api_svnlog_destroy(list_t *list) {
    list_foreach(list,
        lambda void(svn_entry_t *entry) {
            string_destroy(entry->revision);
            string_destroy(entry->author);
            string_destroy(entry->message);
            free(entry);
        }
    );
}

static list_t *module_api_svnlog_read(const char *url, size_t depth) {
    string_t *command = string_format("svn log -l%zu %s", depth, url);
    list_t   *entries = list_create();
    FILE     *fp      = popen(string_contents(command), "r");

    string_destroy(command);
    if (!fp) {
        list_destroy(entries);
        return NULL;
    }

    svn_entry_t *e = NULL;
    for (;;) {
        if (depth == 0)
            break;

        if (!(e = module_api_svnlog_read_entry(fp)))
            break;

        depth--;
        list_push(entries, e);
    }
    pclose(fp);

    return entries;
}

typedef struct {
    unsigned long long num;
    string_t          *str;
} strdur_context_t;

static void module_api_strdur_step(strdur_context_t *ctx, unsigned long long duration, int ch) {
    unsigned long long divide = ctx->num / duration;
    ctx->num %= duration; /* compiler should keep the above div's mod part */
    if (divide)
        string_catf(ctx->str, "%llu%c", divide, ch);
}

static void module_api_dns_destroy(list_t *data) {
    list_foreach(data, lambda void(char *data) => free(data););
    list_destroy(data);
}

/* api interfaces */

#define module_mem_push(MODULE, DATA, FREEFUNC) \
    module_mem_push((MODULE), (DATA), ((void (*)(void *))(FREEFUNC)))

/* list */
list_t *module_api_list_create(void) {
    module_t *module = module_singleton_get();
    list_t *list = list_create();
    if (list)
        module_mem_push(module, list, &list_destroy);
    return list;
}

void *module_api_list_pop(list_t *list) {
    return list_pop(list);
}

void *module_api_list_shift(list_t *list) {
    return list_shift(list);
}

size_t module_api_list_length(list_t *list) {
    return list_length(list);
}

void *module_api_list_at(list_t *list, size_t index) {
    return list_at(list, index);
}

void module_api_list_push(list_t *list, void *element) {
    return list_push(list, element);
}

void module_api_list_sort_impl(list_t *list, bool (*predicate)(const void *, const void *)) {
    return list_sort_impl(list, predicate);
}

void module_api_list_foreach_impl(list_t *list, void *pass, void (*callback)(void *, void *)) {
    return list_foreach_impl(list, pass, callback);
}

void *module_api_list_search_impl(list_t *list, const void *pass, bool (*predicate)(const void *, const void *)) {
    return list_search_impl(list, pass, predicate);
}

/* string */
string_t *module_api_string_create(const char *input) {
    module_t *module = module_singleton_get();
    string_t *string = string_create(input);
    if (string)
        module_mem_push(module, string, &string_destroy);
    return string;
}

string_t *module_api_string_construct(void) {
    module_t *module = module_singleton_get();
    string_t *string = string_construct();
    if (string)
        module_mem_push(module, string, &string_destroy);
    return string;
}

string_t *module_api_string_vformat(const char *input, va_list va) {
    module_t *module = module_singleton_get();
    string_t *string = string_vformat(input, va);
    if (string)
        module_mem_push(module, string, &string_destroy);
    return string;
}

void module_api_string_vcatf(string_t *string, const char *fmt, va_list va) {
    return string_vcatf(string, fmt, va);
}

void module_api_string_shrink(string_t *string, size_t by) {
    return string_shrink(string, by);
}

size_t module_api_string_length(string_t *string) {
    return string_length(string);
}

bool module_api_string_empty(string_t *string) {
    return string_empty(string);
}

char *module_api_string_contents(string_t *string) {
    return string_contents(string);
}

void module_api_string_clear(string_t *string) {
    return string_clear(string);
}

void module_api_string_replace(string_t *string, const char *substr, const char *replace) {
    return string_replace(string, substr, replace);
}

/* hashtable */
hashtable_t *module_api_hashtable_create(size_t size) {
    module_t *module = module_singleton_get();
    hashtable_t *hashtable = hashtable_create(size);
    if (hashtable)
        module_mem_push(module, hashtable, &hashtable_destroy);
    return hashtable;
}

void *module_api_hashtable_find(hashtable_t *hashtable, const char *key) {
    return hashtable_find(hashtable, key);
}

void module_api_hashtable_insert(hashtable_t *hashtable, const char *key, void *value) {
    return hashtable_insert(hashtable, key, value);
}

/* irc */
void module_api_irc_writev(irc_t *irc, const char *channel, const char *fmt, va_list va) {
    return irc_writev(irc, channel, fmt, va);
}

void module_api_irc_actionv(irc_t *irc, const char *channel, const char *fmt, va_list va) {
    return irc_actionv(irc, channel, fmt, va);
}

void module_api_irc_join(irc_t *irc, const char *channel) {
    return irc_join(irc, channel);
}

void module_api_irc_part(irc_t *irc, const char *channel) {
    return irc_part(irc, channel);
}

list_t *module_api_irc_modules_loaded(irc_t *irc) {
    module_t *module = module_singleton_get();
    list_t *list = irc_modules_loaded(irc);
    if (list)
        module_mem_push(module, list, &list_destroy);
    return list;
}

list_t *module_api_irc_modules_enabled(irc_t *irc, const char *channel) {
    module_t *module = module_singleton_get();
    list_t *list = irc_modules_enabled(irc, channel);
    if (list)
        module_mem_push(module, list, &list_destroy);
    return list;
}

hashtable_t *module_api_irc_modules_config(irc_t *irc, const char *channel) {
    module_t *module = module_singleton_get();
    hashtable_t *kvs = module_api_irc_modules_config_copy(irc, module->name, channel);
    if (kvs)
        module_mem_push(module, kvs, &module_api_irc_modules_config_destroy);
    return kvs;
}

list_t *module_api_irc_users(irc_t *irc, const char *channel) {
    module_t *module = module_singleton_get();
    list_t *users = irc_users(irc, channel);
    if (users)
        module_mem_push(module, users, &list_destroy);
    return users;
}

list_t *module_api_irc_channels(irc_t *irc) {
    module_t *module = module_singleton_get();
    list_t *channels = irc_channels(irc);
    if (channels)
        module_mem_push(module, channels, &list_destroy);
    return channels;
}

const char *module_api_irc_nick(irc_t *irc) {
    return irc_nick(irc);
}

const char *module_api_irc_topic(irc_t *irc, const char *channel) {
    return irc_topic(irc, channel);
}

const char *module_api_irc_pattern(irc_t *irc, const char *newpattern) {
    return irc_pattern(irc, newpattern);
}

module_status_t module_api_irc_modules_add(irc_t *irc, const char *file) {
    return irc_modules_add(irc, file);
}

module_status_t module_api_irc_modules_reload(irc_t *irc, const char *name) {
    return irc_modules_reload(irc, name);
}

module_status_t module_api_irc_modules_unload(irc_t *irc, const char *channel, const char *name, bool force) {
    return irc_modules_unload(irc, channel, name, force);
}

module_status_t module_api_irc_modules_disable(irc_t *irc, const char *channel, const char *name) {
    return irc_modules_disable(irc, channel, name);
}

module_status_t module_api_irc_modules_enable(irc_t *irc, const char *channel, const char *name) {
    return irc_modules_enable(irc, channel, name);
}

/* database */
database_statement_t *module_api_database_statement_create(const char *string) {
    module_t *module = module_singleton_get();
    return database_statement_create(module->instance->database, string);
}

bool module_api_database_statement_bindv(database_statement_t *statement, const char *mapping, va_list va) {
    return database_statement_bindv(statement, mapping, va);
}

bool module_api_database_statement_complete(database_statement_t *statement) {
    return database_statement_complete(statement);
}

database_row_t *module_api_database_row_extract(database_statement_t *statement, const char *fields) {
    module_t *module = module_singleton_get();
    database_row_t *row = database_row_extract(statement, fields);
    if (row)
        module_mem_push(module, row, &database_row_destroy);
    return row;
}

char *module_api_database_row_pop_string(database_row_t *row) {
    module_t *module = module_singleton_get();
    char *string = database_row_pop_string(row);
    module_mem_push(module, string, &free);
    return string;
}

int module_api_database_row_pop_integer(database_row_t *row) {
    return database_row_pop_integer(row);
}

bool module_api_database_request(irc_t *instance, const char *table) {
    return database_request(instance, table);
}

int module_api_database_request_count(irc_t *instance, const char *table) {
    return database_request_count(instance, table);
}

/* regex */
regexpr_t *module_api_regexpr_create(const char *string, bool icase) {
    module_t *module = module_singleton_get();
    irc_t *irc = module->instance;
    regexpr_cache_t *cache = irc->regexprcache;
    return regexpr_create(cache, string, icase);
}

bool module_api_regexpr_execute(const regexpr_t *expr, const char *string, size_t nmatch, regexpr_match_t **array) {
    module_t        *module     = module_singleton_get();
    regexpr_match_t *storearray = NULL;

    if (!regexpr_execute(expr, string, nmatch, &storearray))
        return false;

    if (storearray) {
        module_mem_push(module, storearray, &regexpr_execute_destroy);
        *array = storearray;
    }
    return true;
}

/* random */
unsigned int module_api_urand(void) {
    module_t *module = module_singleton_get();
    mt_t *randomdevice = module->random;
    return mt_urand(randomdevice);
}

double module_api_drand(void) {
    module_t *module = module_singleton_get();
    mt_t *randomdevice = module->random;
    return mt_drand(randomdevice);
}

/* redroid */
void module_api_redroid_restart(irc_t *irc, const char *channel, const char *user) {
    return redroid_restart(irc, channel, user);
}

void module_api_redroid_shutdown(irc_t *irc, const char *channel, const char *user) {
    return redroid_shutdown(irc, channel, user);
}

void module_api_redroid_recompile(irc_t *irc, const char *channel, const char *user) {
    return redroid_recompile(irc, channel, user);
}

void module_api_redroid_daemonize(irc_t *irc, const char *channel, const char *user) {
    return redroid_daemonize(irc, channel, user);
}

const char *module_api_redroid_buildinfo(void) {
    return redroid_buildinfo();
}

/* access */
bool module_api_access_range(irc_t *irc, const char *target, int check) {
    return access_range(irc, target, check);
}

bool module_api_access_check(irc_t *irc, const char *target, int check) {
    return access_check(irc, target, check);
}

bool module_api_access_level(irc_t *irc, const char *target, int *level) {
    return access_level(irc, target, level);
}

access_t module_api_access_remove(irc_t *irc, const char *target, const char *invoke) {
    return access_remove(irc, target, invoke);
}

access_t module_api_access_insert(irc_t *irc, const char *target, const char *invoke, int level) {
    return access_insert(irc, target, invoke, level);
}

access_t module_api_access_change(irc_t *irc, const char *target, const char *invoke, int level) {
    return access_change(irc, target, invoke, level);
}

/* misc */
list_t *module_api_strsplit(const char *str_, const char *delim) {
    module_t *module = module_singleton_get();
    list_t *list = list_create();
    module_mem_push(module, list, &list_destroy);

    if (str_ && *str_) {
        char *str = strdup(str_);
        module_mem_push(module, str, &free);
        char *saveptr;
        char *tok = strtok_r(str, delim, &saveptr);
        while (tok) {
            list_push(list, tok);
            tok = strtok_r(NULL, delim, &saveptr);
        }
    }
    return list;
}

list_t *module_api_strnsplit(const char *str_, const char *delim, size_t count) {
    module_t *module = module_singleton_get();
    list_t *list;
    if (str_ && *str_) {
        char *str = strdup(str_);
        module_mem_push(module, str, &free);
        list = module_api_strnsplit_impl(str, delim, count);
    }
    else
        list = list_create();
    module_mem_push(module, list, &list_destroy);
    return list;
}

list_t *module_api_svnlog(const char *url, size_t depth) {
    module_t *module = module_singleton_get();
    list_t *list = module_api_svnlog_read(url, depth);
    if (list) {
        list_t *copy = list_copy(list);
        module_mem_push(module, list, &module_api_svnlog_destroy);
        module_mem_push(module, copy, &list_destroy);
        return copy;
    }
    return NULL;
}

const char *module_api_strdur(unsigned long long duration) {
    if (!duration)
        return "0";

    strdur_context_t ctx = {
        .num = duration,
        .str = string_construct()
    };

                 module_api_strdur_step(&ctx, 60*60*24*7, 'w');
    if (ctx.num) module_api_strdur_step(&ctx, 60*60*24,   'd');
    if (ctx.num) module_api_strdur_step(&ctx, 60*60,      'h');
    if (ctx.num) module_api_strdur_step(&ctx, 60,         'm');
    if (ctx.num) module_api_strdur_step(&ctx, 1,          's');

    module_t *module = module_singleton_get();
    char *move = string_end(ctx.str);
    module_mem_push(module, move, &free);
    return move;
}

list_t *module_api_dns(const char *url) {
    module_t *module = module_singleton_get();

    struct addrinfo *result;
    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    int status;
    if ((status = getaddrinfo(url, NULL, &hints, &result)) != 0)
        return NULL;

    list_t *list = list_create();

    char ipbuffer[INET6_ADDRSTRLEN];
    for (struct addrinfo *p = result; p; p = p->ai_next) {
        void *address;
        if (p->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in*)p->ai_addr;
            address = &ipv4->sin_addr;
        } else {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)p->ai_addr;
            address = &ipv6->sin6_addr;
        }
        inet_ntop(p->ai_family, address, ipbuffer, sizeof(ipbuffer));
        list_push(list, strdup(ipbuffer));
    }
    freeaddrinfo(result);

    if (list_length(list)) {
        list_t *copy = list_copy(list);
        module_mem_push(module, list, &module_api_dns_destroy);
        module_mem_push(module, copy, &list_destroy);
        return copy;
    }

    list_destroy(list);
    return NULL;
}
