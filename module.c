#include "module.h"
#include "database.h"
#include "regexpr.h"
#include "irc.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <regex.h>

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
    //
    // POSIX.1-2003 (Technical Corrigendum 1) see Rational for how this
    // is legal despite it being 'illegal' in C99, which leaves casting
    // from "void *" to a function pointer undefined.
    //
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

    if (!(module->enter = dlsym(module->handle, "module_enter"))) {
        fprintf(stderr, "    module   => missing command handler %s [%s]\n", module->name, module->file);
        return false;
    }

    return true;
}

//
// ensure a module doesn't use any functions by the ones provided by
// a whitelist. This way we can ensure modules don't allocate resources
// with standard library functions.
//
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

    if (!database_statement_bind(statement, "s", name)) {
        database_statement_destroy(statement);
        return false;
    }

    database_row_t *row = database_row_extract(statement, "i");
    if (!row) {
        database_statement_destroy(statement);
        return false;
    }

    *libc = database_row_pop_integer(row);

    if (!database_statement_complete(statement)) {
        database_statement_destroy(statement);
        database_row_destroy(row);
        return false;
    }

    database_row_destroy(row);
    database_statement_destroy(statement);

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

module_t *module_open(const char *file, irc_t *instance, string_t **error) {
    char *function = NULL;
    bool  libc;

    if (!module_allow(file, &function, instance->whitelist, &libc)) {
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
    module->instance = instance;

    if (!module_load(module)) {
        if (!module)
            return NULL;

        if (module->handle)
            dlclose(module->handle);

        free(module->file);
        free(module);
        return NULL;
    }

    return module;
}

bool module_unloaded(irc_t *irc, module_t *test) {
    list_iterator_t *it = list_iterator_create(irc->modunload);
    while (!list_iterator_end(it)) {
        module_t *old = list_iterator_next(it);
        if (old == test) {
            list_iterator_destroy(it);
            return true;
        }
    }
    list_iterator_destroy(it);
    return false;
}

bool module_reload(module_t *module) {
    if (module->close)
        module->close(module->instance);
    dlclose(module->handle);

    // save old address for unloaded modules
    list_push(module->instance->modunload, module->handle);

    if (!(module->handle = dlopen(module->file, RTLD_LAZY)))
        goto module_reload_error;

    if (!module_load(module))
        goto module_reload_error;

    return true;

module_reload_error:
    module_destroy(module);
    return false;
}

void module_destroy(module_t *module) {
    if (module->close)
        module->close(module->instance);
    if (module->handle)
        dlclose(module->handle);
    free(module->file);

    // save old address for unloaded modules
    list_push(module->instance->modunload, module);

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

// memory pinners
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
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    database_statement_t *statement = database_statement_create(module->instance->database, string);
    module_mem_push(module, statement, (void(*)(void*))&database_statement_destroy);
    module_mem_mutex_unlock(module);
    return statement;
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

list_t *module_irc_modules_list(irc_t *irc) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    list_t *ret = irc_modules_list(irc);
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

list_t* module_strsplit(char *str, char *delim) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    list_t *list = list_create();
    module_mem_push(module, list, (void (*)(void*))&list_destroy);
    char *saveptr;
    char *tok = strtok_r(str, delim, &saveptr);
    while (tok) {
        list_push(list, tok);
        tok = strtok_r(NULL, delim, &saveptr);
    }
    module_mem_mutex_unlock(module);
    return list;
}

list_t* module_strnsplit(char *str, char *delim, size_t count) {
    module_t *module = *module_get();
    module_mem_mutex_lock(module);
    list_t *list = list_create();
    module_mem_push(module, list, (void (*)(void*))&list_destroy);
    if (count < 2) {
        while (*str && strchr(delim, *str))
            ++str;
        if (*str)
            list_push(list, str);
    }
    else {
        char *saveptr;
        char *tok = strtok_r(str, delim, &saveptr);
        while (tok) {
            list_push(list, tok);
            if (!--count) {
                tok += strlen(tok)+1;
                while (*tok && strchr(delim, *tok))
                    ++tok;
                if (*tok)
                    list_push(list, tok);
                return list;
            }
            tok = strtok_r(NULL, delim, &saveptr);
        }
    }
    module_mem_mutex_unlock(module);
    return list;
}


regexpr_t *module_regexpr_create(const char *string, bool icase) {
    //
    // There is no need to gaurd a mutex here since the module cache
    // system also serves as a garbage collector. This will implictly
    // deal with freeing regexpr_t objects automatically. Instead we
    // need the GC call to access the modules instances regexpr cache.
    //
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
