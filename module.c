#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#include <pthread.h>
#include <dlfcn.h>  /* dlsym, dlopen, RTLD_LAZY, dlerror, dlclose */
#include <netdb.h>  /* getaddrinfo */
#include <elf.h>

#include "module.h"

/* A simplified GC because we only enter this from one thread */
typedef struct {
    void (*callback)(void *data);
    void  *data;
} module_mem_node_t;

static module_mem_node_t *module_mem_node_create(void *data, void (*callback)(void*)) {
    module_mem_node_t *node = malloc(sizeof(*node));
    node->data     = data;
    node->callback = callback;
    return node;
}

static void module_mem_node_destroy(module_mem_node_t *node) {
    if (node->callback)
        node->callback(node->data);
    free(node);
}

void module_mem_destroy(module_t *module) {
    list_foreach(module->memory, NULL, &module_mem_node_destroy);
    list_destroy(module->memory);
}

void module_mem_push(module_t *module, void *data, void (*callback)(void *)) {
    module_mem_node_t *node = module_mem_node_create(data, callback);
    list_push(module->memory, node);
}

static bool module_load(module_t *module) {
    /*
     * POSIX.1-2003 (Technical Corrigendum 1) see Rational for how this
     * is legal despite it being 'illegal' in C99, which leaves casting
     * from "void *" to a function pointer undefined.
     */

    *(void **)(&module->enter) = dlsym(module->handle, "module_enter");
    *(void **)(&module->close) = dlsym(module->handle, "module_close");

    if (!(module->name = (const char *)dlsym(module->handle, "module_name"))) {
        fprintf(stderr, "    module   => missing module name in `%s'\n", module->file);
        return false;
    }

    if (!(module->match = dlsym(module->handle, "module_match"))) {
        fprintf(stderr, "    module   => missing command match rule %s [%s]\n", module->name, module->file);
        return false;
    }

    if (!module->enter) {
        fprintf(stderr, "    module   => missing command handler %s [%s]\n", module->name, module->file);
        return false;
    }

    int *interval = dlsym(module->handle, "module_interval");

    module->interval     = (interval) ? *interval : 0;
    module->lastinterval = 0;

    return true;
}

/*
 * ensure a module doesn't use any functions by the ones provided by
 * a whitelist. This way we can ensure modules don't allocate resources
 * with standard library functions.
 */
#if ULONG_MAX == 0xffffffff
    typedef Elf32_Ehdr Elf_Ehdr;
    typedef Elf32_Phdr Elf_Phdr;
    typedef Elf32_Shdr Elf_Shdr;
    typedef Elf32_Sym  Elf_Sym;
#   define FUN(X) ELF32_ST_TYPE(X)
#else
    typedef Elf64_Ehdr Elf_Ehdr;
    typedef Elf64_Phdr Elf_Phdr;
    typedef Elf64_Shdr Elf_Shdr;
    typedef Elf64_Sym  Elf_Sym;
#   define FUN(X) ELF64_ST_TYPE(X)
#endif

static bool module_allow_symbol(const char *name, database_t *database, bool *libc) {
    if (!name || !*name || *name == '_' || !strncmp(name, "module_", 7))
        return true;

    database_statement_t *statement = database_statement_create(database, "SELECT LIBC FROM WHITELIST WHERE NAME=?");
    if (!statement)
        return false;

    if (!database_statement_bind(statement, "s", name))
        return false;

    database_row_t *row = database_row_extract(statement, "i");
    if (!row)
        return false;

    *libc = database_row_pop_integer(row);

    if (!database_statement_complete(statement)) {
        database_row_destroy(row);
        return false;
    }

    database_row_destroy(row);

    return true;
}

static bool module_allow(const char *path, char **function, database_t *database, bool *libc) {
    void     *base        = NULL;
    size_t    size        = 0;
    Elf_Ehdr *ehdr        = NULL;
    Elf_Shdr *shdr        = NULL;
    Elf_Sym  *dsymtab     = NULL;
    Elf_Sym  *dsymtab_end = NULL;
    char     *dstrtab     = NULL;
    FILE     *file        = fopen(path, "r");

    if (!file)
        return false;

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    base = malloc(size);
    fseek(file, 0, SEEK_SET);
    fread(base, size, 1, file);
    fclose(file);

    ehdr = base;
    shdr = (Elf_Shdr*)(base + ehdr->e_shoff);
    for (size_t i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_DYNSYM) {
            dsymtab     = (Elf_Sym*)(base + shdr[i].sh_offset);
            dsymtab_end = (Elf_Sym*)((char *)dsymtab + shdr[i].sh_size);
            dstrtab     = (char *)(base + shdr[shdr[i].sh_link].sh_offset);
        }
    }

    while (dsymtab < dsymtab_end) {
        if (FUN(dsymtab->st_info) == STT_FUNC || FUN(dsymtab->st_info) == STT_NOTYPE) {
            if (!module_allow_symbol(&dstrtab[dsymtab->st_name], database, libc)) {
                *function = strdup(&dstrtab[dsymtab->st_name]);
                free(base);
                return false;
            }
        }
        dsymtab++;
    }

    free(base);
    return true;
}

module_t *module_open(const char *file, module_manager_t *manager, string_t **error) {
    char *function = NULL;
    bool  libc     = false;

    if (!module_allow(file, &function, manager->whitelist, &libc)) {
        *error = string_construct();
        if (libc)
            string_catf(*error, "%s from libc is blacklisted", function);
        else
            string_catf(*error, "%s blacklisted", function);
        free(function);
        return NULL;
    }

    module_t *module = malloc(sizeof(*module));

    if (!(module->handle = dlopen(file, RTLD_LAZY))) {
        free(module);
        *error = string_create(dlerror());
        return NULL;
    }

    module->file     = strdup(file);
    module->instance = manager->instance;

    if (!module_load(module)) {
        if (!module)
            return NULL;

        if (module->handle)
            dlclose(module->handle);

        free(module->file);
        free(module);
        return NULL;
    }

    module->random = mt_create();
    list_push(manager->modules, module);
    return module;
}

bool module_reload(module_t *module, module_manager_t *manager) {
    if (module->close)
        module->close(module->instance);
    dlclose(module->handle);

    /* Save old address for unloaded module */
    list_push(manager->unloaded, module->handle);

    if (!(module->handle = dlopen(module->file, RTLD_LAZY)))
        goto module_reload_error;

    if (!module_load(module))
        goto module_reload_error;

    /* Reload the PRNG as well */
    mt_destroy(module->random);
    module->random = mt_create();

    return true;

module_reload_error:
    module_close(module, manager);
    return false;
}

void module_close(module_t *module, module_manager_t *manager) {
    if (module->close)
        module->close(module->instance);
    if (module->handle)
        dlclose(module->handle);
    free(module->file);

    /* Save old address for unloaded module */
    list_push(manager->unloaded, module);
    mt_destroy(module->random);
    free(module);
}

/* thread-safe module singleton */
typedef struct {
    pthread_mutex_t mutex;
    module_t       *handle;
} module_get_t;

static module_get_t *module_singleton(void) {
    static module_get_t get = {
        .mutex  = PTHREAD_MUTEX_INITIALIZER,
        .handle = NULL
    };
    return &get;
}

void module_singleton_set(module_t *module) {
    module_get_t *get = module_singleton();
    pthread_mutex_lock(&get->mutex);
    get->handle = module;
    pthread_mutex_unlock(&get->mutex);
}

module_t *module_singleton_get(void) {
    module_get_t *get = module_singleton();
    module_t *mod = NULL;
    pthread_mutex_lock(&get->mutex);
    mod = get->handle;
    pthread_mutex_unlock(&get->mutex);
    return mod;
}

/* Memory pinners for module API */
void *module_malloc(size_t bytes) {
    module_t *module = module_singleton_get();
    void *p = memset(malloc(bytes), 0, bytes);
    module_mem_push(module, p, &free);
    return p;
}

string_t *module_string_create(const char *input) {
    module_t *module = module_singleton_get();
    string_t *string = string_create(input);
    module_mem_push(module, string, (void (*)(void *))&string_destroy);
    return string;
}

string_t *module_string_construct(void) {
    module_t *module = module_singleton_get();
    string_t *string = string_construct();
    module_mem_push(module, string, (void (*)(void *))&string_destroy);
    return string;
}

string_t *module_string_vformat(const char *fmt, va_list va) {
    module_t *module = module_singleton_get();
    string_t *string = string_vformat(fmt, va);
    module_mem_push(module, string, (void (*)(void*))&string_destroy);
    return string;
}

list_t *module_list_create(void) {
    module_t *module = module_singleton_get();
    list_t *list = list_create();
    module_mem_push(module, list, (void (*)(void*))&list_destroy);
    return list;
}

int module_getaddrinfo(const char *mode, const char *service, const struct addrinfo *hints, struct addrinfo **result) {
    module_t *module = module_singleton_get();
    int value = getaddrinfo(mode, service, hints, result);
    if (value == 0)
        module_mem_push(module, *result, (void (*)(void*))&freeaddrinfo);
    return value;
}

database_statement_t *module_database_statement_create(const char *string) {
    /*
     * There is no need to gaurd a mutex here since the database statement
     * cache system also serves as a garbage collector. This will implictly
     * deal with freeing database statements. Instead we need the GC call
     * to access the modules instance database
     */
    return database_statement_create((module_singleton_get())->instance->database, string);
}

database_row_t *module_database_row_extract(database_statement_t *statement, const char *fields) {
    module_t *module = module_singleton_get();
    database_row_t *row = database_row_extract(statement, fields);
    if (row)
        module_mem_push(module, row, (void(*)(void*))&database_row_destroy);
    return row;
}

const char *module_database_row_pop_string(database_row_t *row) {
    module_t *module = module_singleton_get();
    const char *ret = database_row_pop_string(row);
    module_mem_push(module, (void *)ret, (void(*)(void*))&free);
    return ret;
}

list_t *module_irc_modules_loaded(irc_t *irc) {
    module_t *module = module_singleton_get();
    list_t *ret = irc_modules_loaded(irc);
    if (!ret)
        return NULL;
    module_mem_push(module, (void*)ret, (void(*)(void*))&list_destroy);
    return ret;
}

list_t *module_irc_modules_enabled(irc_t *irc, const char *channel) {
    module_t *module = module_singleton_get();
    list_t *ret = irc_modules_enabled(irc, channel);
    if (!ret)
        return NULL;
    module_mem_push(module, (void *)ret, (void (*)(void*))&list_destroy);
    return ret;
}

list_t *module_irc_users(irc_t *irc, const char *channel) {
    module_t *module = module_singleton_get();
    list_t *ret = irc_users(irc, channel);
    if (!ret)
        return NULL;
    module_mem_push(module, (void*)ret, (void(*)(void*))&list_destroy);
    return ret;
}

list_t *module_irc_channels(irc_t *irc) {
    module_t *module = module_singleton_get();
    list_t *ret = irc_channels(irc);
    if (!ret)
        return NULL;
    module_mem_push(module, (void*)ret, (void(*)(void*))&list_destroy);
    return ret;
}

char *module_strdup(const char *str) {
    module_t *module = module_singleton_get();
    char *dup = strdup(str);
    module_mem_push(module, dup, &free);
    return dup;
}

list_t* module_strsplit(const char *str_, const char *delim) {
    module_t *module = module_singleton_get();
    list_t *list = list_create();
    module_mem_push(module, list, (void (*)(void*))&list_destroy);

    if (str_ && *str_) {
        char *str = strdup(str_);
        char *saveptr;
        char *tok = strtok_r(str, delim, &saveptr);
        while (tok) {
            list_push(list, tok);
            tok = strtok_r(NULL, delim, &saveptr);
        }
    }
    return list;
}

list_t* module_strnsplit_impl(char *str, const char *delim, size_t count) {
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

list_t* module_strnsplit(const char *str_, const char *delim, size_t count) {
    module_t *module = module_singleton_get();

    list_t *list;
    if (str_ && *str_) {
        char *str = strdup(str_);
        module_mem_push(module, str, &free);
        list = module_strnsplit_impl(str, delim, count);
    }
    else
        list = list_create();
    module_mem_push(module, list, (void (*)(void*))&list_destroy);
    return list;
}

typedef struct {
    unsigned long long num;
    string_t          *str;
} strdur_context_t;

static void strdur_step(strdur_context_t *c, unsigned long long d, const char *suffix) {
    unsigned long long cnt = c->num / d;
    c->num %= d; /* compiler should keep the above div's mod part */
    if (!cnt)
        return;
    string_catf(c->str, "%llu%s", cnt, suffix);
}

static char *strdur(unsigned long long duration) {
    if (!duration)
        return strdup("0");

    strdur_context_t ctx = {
        .num = duration,
        .str = string_construct()
    };

                 strdur_step(&ctx, 60*60*24*7, "w");
    if (ctx.num) strdur_step(&ctx, 60*60*24,   "d");
    if (ctx.num) strdur_step(&ctx, 60*60,      "h");
    if (ctx.num) strdur_step(&ctx, 60,         "m");
    if (ctx.num) strdur_step(&ctx, 1,          "s");

    return string_end(ctx.str);
}

char *module_strdur(unsigned long long duration) {
    module_t *module = module_singleton_get();
    char *data = strdur(duration);
    module_mem_push(module, data, &free);
    return data;
}

regexpr_t *module_regexpr_create(const char *string, bool icase) {
    /*
     * There is no need to gaurd a mutex here since the module cache
     * system also serves as a garbage collector. This will implictly
     * deal with freeing regexpr_t objects automatically. Instead we
     * need the GC call to access the modules instances regexpr cache.
     */
    module_t *module = module_singleton_get();
    return regexpr_create(module->instance->regexprcache, string, icase);
}

bool module_regexpr_execute(const regexpr_t *expr, const char *string, size_t nmatch, regexpr_match_t **array) {
    module_t        *module     = module_singleton_get();
    regexpr_match_t *storearray = NULL;

    if (!regexpr_execute(expr, string, nmatch, &storearray))
        return false;

    if (storearray) {
        module_mem_push(module, storearray, (void (*)(void *))&regexpr_execute_destroy);
        *array = storearray;
    }
    return true;
}

typedef struct {
    string_t *message;
    string_t *author;
    string_t *revision;
} svn_entry_t;

/* a simple way to parse the svn log */
static svn_entry_t *module_svnlog_read_entry(FILE *handle) {
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
    list_t *split = module_strnsplit_impl(copy, " |", 2);
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

static void module_svnlog_destroy(list_t *list) {
    list_foreach(list, NULL,
        lambda void(svn_entry_t *entry) {
            string_destroy(entry->revision);
            string_destroy(entry->author);
            string_destroy(entry->message);
            free(entry);
        }
    );
}

static list_t *module_svnlog_read(const char *url, size_t depth) {
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

        if (!(e = module_svnlog_read_entry(fp)))
            break;

        depth--;
        list_push(entries, e);
    }
    pclose(fp);

    return entries;
}

list_t *module_svnlog(const char *url, size_t depth) {
    module_t *module = module_singleton_get();
    list_t *list = module_svnlog_read(url, depth);
    if (!list)
        return NULL;

    /*
     * A copy of the list needs to be created because a module may pop
     * or modify the contents of the list which is used in the cleanup
     * to free resources. This is bad because it means memory leaks are
     * possible.
     */
    list_t *copy = list_copy(list);
    module_mem_push(module, list, (void(*)(void* ))&module_svnlog_destroy);
    module_mem_push(module, copy, (void(*)(void* ))&list_destroy);
    return copy;
}

uint32_t module_urand(void) {
    return mt_urand((module_singleton_get())->random);
}

double module_drand(void) {
    return mt_drand((module_singleton_get())->random);
}

static hashtable_t *module_irc_modules_config_copy(irc_t *irc, const char *mname, const char *cname) {
    irc_channel_t *channel = hashtable_find(irc->channels, cname);
    if (!channel)
        return NULL;
    irc_module_t *module = hashtable_find(channel->modules, mname);
    if (!module)
        return NULL;
    return hashtable_copy(module->kvs, &strdup);
}

static void module_irc_modules_config_destroy(hashtable_t *kvs) {
    hashtable_foreach(kvs, NULL, &free);
    hashtable_destroy(kvs);
}

hashtable_t *module_irc_modules_config(irc_t *irc, const char *channel) {
    module_t *module = module_singleton_get();
    hashtable_t *kvs = module_irc_modules_config_copy(irc, module->name, channel);
    if (!kvs)
        return NULL;
    module_mem_push(module, kvs, (void(*)(void*))&module_irc_modules_config_destroy);
    return kvs;
}
