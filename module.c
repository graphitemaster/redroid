#include "irc.h"
#include "module.h"
#include "database.h"
#include "regexpr.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <regex.h>
#include <limits.h>

struct module_mem_s {
    module_t          *instance;
    module_mem_node_t *head;
    module_mem_node_t *tail;
    pthread_mutex_t    mutex;
};

static module_mem_node_t *module_mem_node_create(void) {
    return memset(malloc(sizeof(module_mem_node_t)), 0, sizeof(module_mem_node_t));
}

static void module_mem_node_destroy(module_mem_node_t *node) {
    if (node->cleanup)
        node->cleanup(node->data);
    free(node);
}

module_mem_t *module_mem_create(module_t *instance) {
    module_mem_t *mem = malloc(sizeof(*mem));

    mem->instance = instance;
    mem->head     = module_mem_node_create();
    mem->tail     = mem->head;

    pthread_mutex_init(&mem->mutex, NULL);

    return mem;
}

void module_mem_push(module_t *module, void *data, void (*cleanup)(void *)) {
    module_mem_t *mem  = module->memory;
    mem->tail->next    = module_mem_node_create();
    mem->tail->data    = data;
    mem->tail->cleanup = cleanup;
    mem->tail          = mem->tail->next;
}

static bool module_mem_pop(module_mem_t *mem) {
    if (!mem->head->data)
        return false;

    module_mem_node_t *temp = mem->head->next;
    module_mem_node_destroy(mem->head);
    mem->head = temp;

    return true;
}

void module_mem_destroy(module_t *module) {
    module_mem_t *mem = module->memory;
    while (module_mem_pop(mem))
        ;
    module_mem_node_destroy(mem->head);
    pthread_mutex_destroy(&mem->mutex);
    free(mem);
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
        fprintf(stderr, "    module   => missing module name in `%s`\n", module->file);
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
#include <elf.h>
#include <stdint.h>
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
    void     *base;
    size_t    size;
    Elf_Ehdr *ehdr;
    Elf_Shdr *shdr;
    Elf_Sym  *dsymtab;
    Elf_Sym  *dsymtab_end;
    char     *dstrtab;
    FILE     *file = fopen(path, "r");

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
    bool  libc;

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

void module_mem_mutex_lock(module_t *module) {
    pthread_mutex_lock(&module->memory->mutex);
}

void module_mem_mutex_unlock(module_t *module) {
    pthread_mutex_unlock(&module->memory->mutex);
}

module_t **module_get(void) {
    static module_t *module = NULL;
    return &module;
}

module_manager_t *module_manager_create(irc_t *instance) {
    module_manager_t *manager = malloc(sizeof(*manager));

    manager->instance  = instance;
    manager->modules   = list_create();
    manager->unloaded  = list_create();
    manager->whitelist = database_create("whitelist.db");

    return manager;
}

void module_manager_destroy(module_manager_t *manager) {
    list_foreach(manager->modules, manager, &module_close);
    list_destroy(manager->modules);
    list_destroy(manager->unloaded);

    database_destroy(manager->whitelist);

    free(manager);
}

bool module_manager_module_unload(module_manager_t *manager, const char *name) {
    module_t *find = module_manager_module_search(manager, name, MMSEARCH_NAME);
    if (!find)
        return false;

    if (!list_erase(manager->modules, find))
        return false;

    return true;
}

bool module_manager_module_reload(module_manager_t *manager, const char *name) {
    module_t *find = module_manager_module_search(manager, name, MMSEARCH_NAME);
    if (!find)
        return false;

    if (!module_reload(find, manager))
        return false;

    return true;
}

bool module_manager_module_unloaded(module_manager_t *manager, module_t *module) {
    list_iterator_t *it = list_iterator_create(manager->unloaded);
    while (!list_iterator_end(it)) {
        module_t *instance = list_iterator_next(it);
        if (instance == module) {
            list_iterator_destroy(it);
            return true;
        }
    }
    list_iterator_destroy(it);
    return false;
}

module_t *module_manager_module_command(module_manager_t *manager, const char *command) {
    return module_manager_module_search(manager, command, MMSEARCH_MATCH);
}

module_t *module_manager_module_search(module_manager_t *manager, const char *name, int method) {
    list_iterator_t *it;

    for (it = list_iterator_create(manager->modules); !list_iterator_end(it); ) {
        module_t   *module = list_iterator_next(it);
        const char *compare;

        switch (method) {
            case MMSEARCH_FILE:  compare = module->file;  break;
            case MMSEARCH_NAME:  compare = module->name;  break;
            case MMSEARCH_MATCH: compare = module->match; break;

            default:
                list_iterator_destroy(it);
                return NULL;
        }

        if (!strcmp(compare, name)) {
            list_iterator_destroy(it);
            return module;
        }
    }
    list_iterator_destroy(it);
    return NULL;
}

/* Memory pinners for module API */
void *module_malloc(size_t bytes) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    void *p = memset(malloc(bytes), 0, bytes);
    module_mem_push(module, p, &free);
    module_mem_mutex_unlock(module);
    return p;
}

string_t *module_string_create(const char *input) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    string_t *string = string_create(input);
    module_mem_push(module, string, (void (*)(void *))&string_destroy);
    module_mem_mutex_unlock(module);
    return string;
}

string_t *module_string_construct(void) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    string_t *string = string_construct();
    module_mem_push(module, string, (void (*)(void *))&string_destroy);
    module_mem_mutex_unlock(module);
    return string;
}

string_t *module_string_vformat(const char *fmt, va_list va) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    string_t *string = string_vformat(fmt, va);
    module_mem_push(module, string, (void (*)(void*))&string_destroy);
    module_mem_mutex_unlock(module);
    return string;
}

list_iterator_t *module_list_iterator_create(list_t *list) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    list_iterator_t *it = list_iterator_create(list);
    module_mem_push(module, it, (void (*)(void*))&list_iterator_destroy);
    module_mem_mutex_unlock(module);
    return it;
}

list_t *module_list_create(void) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    list_t *list = list_create();
    module_mem_push(module, list, (void (*)(void*))&list_destroy);
    module_mem_mutex_unlock(module);
    return list;
}

int module_getaddrinfo(const char *mode, const char *service, const struct addrinfo *hints, struct addrinfo **result) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    int value = getaddrinfo(mode, service, hints, result);
    if (value == 0)
        module_mem_push(module, *result, (void (*)(void*))&freeaddrinfo);
    module_mem_mutex_unlock(module);
    return value;
}

database_statement_t *module_database_statement_create(const char *string) {
    /*
     * There is no need to gaurd a mutex here since the database statement
     * cache system also serves as a garbage collector. This will implictly
     * deal with freeing database statements. Instead we need the GC call
     * to access the modules instance database
     */
    return database_statement_create((*module_get())->instance->database, string);
}

database_row_t *module_database_row_extract(database_statement_t *statement, const char *fields) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    database_row_t *row = database_row_extract(statement, fields);
    if (row)
        module_mem_push(module, row, (void(*)(void*))&database_row_destroy);
    module_mem_mutex_unlock(module);
    return row;
}

const char *module_database_row_pop_string(database_row_t *row) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    const char *ret = database_row_pop_string(row);
    module_mem_push(module, (void *)ret, (void(*)(void*))&free);
    module_mem_mutex_unlock(module);
    return ret;
}

list_t *module_irc_modules(irc_t *irc) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    list_t *ret = irc_modules(irc);
    if (!ret) {
        module_mem_mutex_unlock(module);
        return NULL;
    }
    module_mem_push(module, (void*)ret, (void(*)(void*))&list_destroy);
    module_mem_mutex_unlock(module);
    return ret;
}

list_t *module_irc_users(irc_t *irc, const char *channel) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    list_t *ret = irc_users(irc, channel);
    if (!ret) {
        module_mem_mutex_unlock(module);
        return NULL;
    }
    module_mem_push(module, (void*)ret, (void(*)(void*))&list_destroy);
    module_mem_mutex_unlock(module);
    return ret;
}

list_t *module_irc_channels(irc_t *irc) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    list_t *ret = irc_channels(irc);
    if (!ret) {
        module_mem_mutex_unlock(module);
        return NULL;
    }
    module_mem_push(module, (void*)ret, (void(*)(void*))&list_destroy);
    module_mem_mutex_unlock(module);
    return ret;
}

void module_list_push(list_t *list, void *element) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    list_push(list, element);
    module_mem_mutex_unlock(module);
}

char *module_strdup(const char *str) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    char *dup = strdup(str);
    if (dup)
        module_mem_push(module, dup, &free);
    module_mem_mutex_unlock(module);
    return dup;
}

list_t* module_strsplit(const char *str_, const char *delim) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);

    list_t *list = list_create();
    module_mem_push(module, list, (void (*)(void*))&list_destroy);

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

    module_mem_mutex_unlock(module);
    return list;
}

list_t* module_strnsplit_impl(char *str, const char *delim, size_t count) {
    list_t *list = list_create();
    if (count < 2) {
        while (*str && strchr(delim, *str))
            ++str;
        if (*str)
            list_push(list, str);
        return list;
    }
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
    module_t *module = *module_get();
    module_mem_mutex_lock(module);

    list_t *list;
    if (str_ && *str_) {
        char *str = strdup(str_);
        module_mem_push(module, str, &free);
        list = module_strnsplit_impl(str, delim, count);
    }
    else
        list = list_create();

    module_mem_push(module, list, (void (*)(void*))&list_destroy);

    module_mem_mutex_unlock(module);
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
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    char *data = strdur(duration);
    module_mem_push(module, data, &free);
    module_mem_mutex_unlock(module);
    return data;
}

regexpr_t *module_regexpr_create(const char *string, bool icase) {
    /*
     * There is no need to gaurd a mutex here since the module cache
     * system also serves as a garbage collector. This will implictly
     * deal with freeing regexpr_t objects automatically. Instead we
     * need the GC call to access the modules instances regexpr cache.
     */
    module_t *module = *module_get();
    return regexpr_create(module->instance->regexprcache, string, icase);
}

bool module_regexpr_execute(const regexpr_t *expr, const char *string, size_t nmatch, regexpr_match_t **array) {
    module_t        *module     = *module_get();
    regexpr_match_t *storearray = NULL;

    module_mem_mutex_lock(module);
    if (!regexpr_execute(expr, string, nmatch, &storearray)) {
        module_mem_mutex_unlock(module);
        return false;
    }

    if (storearray) {
        module_mem_push(module, storearray, (void (*)(void *))&regexpr_execute_destroy);
        *array = storearray;
    }

    module_mem_mutex_unlock(module);
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
    list_iterator_t *it;
    for (it = list_iterator_create(list); !list_iterator_end(it);) {
        svn_entry_t *e = list_iterator_next(it);
        string_destroy(e->revision);
        string_destroy(e->author);
        string_destroy(e->message);
        free(e);
    }
    list_iterator_destroy(it);
    list_destroy(list);
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
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    list_t *list = module_svnlog_read(url, depth);
    if (!list) {
        module_mem_mutex_unlock(module);
        return NULL;
    }

    /*
     * A copy of the list needs to be created because a module may pop
     * or modify the contents of the list which is used in the cleanup
     * to free resources. This is bad because it means memory leaks are
     * possible.
     */
    list_t *copy = list_copy(list);
    module_mem_push(module, list, (void(*)(void* ))&module_svnlog_destroy);
    module_mem_push(module, copy, (void(*)(void* ))&list_destroy);
    module_mem_mutex_unlock(module);
    return copy;
}

uint32_t module_urand(void) {
    return mt_urand((*module_get())->random);
}

double module_drand(void) {
    return mt_drand((*module_get())->random);
}
