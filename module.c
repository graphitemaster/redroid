#include "module.h"
#include "irc.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>

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

void module_mem_push(module_mem_t *mem, void *data, void (*cleanup)(void *)) {
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

void module_mem_destroy(module_mem_t *mem) {
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

static bool module_allow_symbol(const char *name) {
    if (!name || !*name || !strncmp(name, "module_", 7))
        return true;

    static const char *list[] = {
        // ctype.h
        "isalnum",    "isalpha",  "islower",   "isupper",    "isdigit",
        "isxdigit",   "iscntrl",  "isgraph",   "isspace",    "isblank",
        "isprint",    "ispunct",  "tolower",   "toupper",
        // math.h
        "abs",        "labs",     "llabs",     "fabs",       "div",
        "ldiv",       "lldiv",    "fmod",      "remainder",  "remquo",
        "fma",        "fmax",     "fmin",      "fdim",       "nan",
        "nan",        "nanl",     "exp",       "exp2",       "expm1",
        "log",        "log2",     "log10",     "log1p",      "ilogb",
        "logb",       "sqrt",     "cbrt",      "hypot",      "pow",
        "sin",        "cos",      "tan",       "asin",       "acos",
        "atan",       "atan2",    "sinh",      "cosh",       "tanh",
        "asinh",      "acosh",    "atanh",     "erf",        "erfc",
        "lgamma",     "tgamma",   "ceil",      "floor",      "trunc",
        "round",      "lround",   "llround",   "nearbyint",  "rint",
        "lrint",      "llrint",   "frexp",     "ldexp",      "modf",
        "scalbn",     "scalbln",  "nextafter", "nexttoward", "copysign",
        "fpclassify", "isfinite", "isinf",     "isnan",      "isnormal",
        "signbit",
        // stdio.h
        "sscanf",     "vsscanf",  "sprintf",   "snprintf",   "vsprintf",
        "vsnprintf",
        // stdlib.h
        "abs",        "atof",     "atoi",      "atol",       "atoll",
        "div",        "getenv",   "labs",      "ldiv",       "llabs",
        "lldiv",      "qsort",    "strtod",    "strtof",     "strtol",
        "strtold",    "strtoll",  "strtoul",   "strtoull",   "rand",
        // string.h
        "strcpy",     "strncpy",  "strcat",    "strncat",    "strxfrm",
        "strlen",     "strcmp",   "strncmp",   "strcoll",    "strchr",
        "strrchr",    "strspn",   "strcspn",   "strpbrk",    "strstr",
        "strtok",     "memset",   "memcpy",    "memmove",    "memcmp",
        "memchr",
        // time.h
        "difftime",   "time",     "clock",     "asctime",    "ctime",
        "strftime",   "gmtime",   "localtime", "mktime",

        // list.h
        "list_iterator_next",
        "list_iterator_end",
        "list_pop",
        "list_length",

        // string.h
        "string_contents",
        "string_length",
        "string_catf",

        // irc.h
        "irc_modules_add",
        "irc_action",
        "irc_write",
        "irc_nick",

        // database.h
        "database_statement_complete",
        "database_statement_bind",
        "database_request",
        "database_request_count",
        "database_row_pop_integer",

        // misc
        "inet_ntop",
        "raise"
    };

    for (size_t i = 0; i < sizeof(list) / sizeof(*list); i++)
        if (!strcmp(list[i], name) || *name == '_')
            return true;

    return false;
}

static bool module_allow(const char *path, char **function) {
    void     *base;
    size_t    size;
    Elf_Ehdr *ehdr;
    Elf_Shdr *shdr;
    Elf_Sym  *dsymtab;
    Elf_Sym  *dsymtab_end;
    char     *dstrtab;
    FILE     *file = fopen(path, "r");

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
        if (FUN(dsymtab->st_info) == STT_FUNC ||
            FUN(dsymtab->st_info) == STT_NOTYPE) // no type functions are hard
        {
            if (!module_allow_symbol(&dstrtab[dsymtab->st_name])) {
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
    if (!module_allow(file, &function)) {
        *error = string_construct();
        string_catf(*error, "%s is blacklisted", function);
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

bool module_reload(module_t *module) {
    if (module->close)
        module->close(module);
    dlclose(module->handle);
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
        module->close(module);
    if (module->handle)
        dlclose(module->handle);
    free(module->file);
    free(module);
}

void module_mem_mutex_lock(module_t *module) {
    pthread_mutex_lock(&module->memory->mutex);
}

void module_mem_mutex_unlock(module_t *module) {
    pthread_mutex_unlock(&module->memory->mutex);
}

// memory pinners for module garbage collection
void *module_malloc(module_t *module, size_t bytes) {
    module_mem_mutex_lock(module);
    void *p = malloc(bytes);
    module_mem_push(module->memory, p, &free);
    module_mem_mutex_unlock(module);
    return p;
}

string_t *module_string_create(module_t *module, const char *input) {
    module_mem_mutex_lock(module);
    string_t *string = string_create(input);
    module_mem_push(module->memory, string, (void (*)(void *))&string_destroy);
    module_mem_mutex_unlock(module);
    return string;
}

string_t *module_string_construct(module_t *module) {
    module_mem_mutex_lock(module);
    string_t *string = string_construct();
    module_mem_push(module->memory, string, (void (*)(void *))&string_destroy);
    module_mem_mutex_unlock(module);
    return string;
}

list_iterator_t *module_list_iterator_create(module_t *module, list_t *list) {
    module_mem_mutex_lock(module);
    list_iterator_t *it = list_iterator_create(list);
    module_mem_push(module->memory, it, (void (*)(void*))&list_iterator_destroy);
    module_mem_mutex_unlock(module);
    return it;
}

list_t *module_list_create(module_t *module) {
    module_mem_mutex_lock(module);
    list_t *list = list_create();
    module_mem_push(module->memory, list, (void (*)(void*))&list_destroy);
    module_mem_mutex_unlock(module);
    return list;
}

int module_getaddrinfo(module_t *module, const char *mode, const char *service, const struct addrinfo *hints, struct addrinfo **result) {
    module_mem_mutex_lock(module);
    int value = getaddrinfo(mode, service, hints, result);
    if (value == 0)
        module_mem_push(module->memory, *result, (void (*)(void*))&freeaddrinfo);
    module_mem_mutex_unlock(module);
    return value;
}

database_statement_t *module_database_statement_create(module_t *module, const char *string) {
    module_mem_mutex_lock(module);
    database_statement_t *statement = database_statement_create(module->instance->database, string);
    module_mem_push(module->memory, statement, (void(*)(void*))&database_statement_destroy);
    module_mem_mutex_unlock(module);
    return statement;
}

database_row_t *module_database_row_extract(module_t *module, database_statement_t *statement, const char *fields) {
    module_mem_mutex_lock(module);
    database_row_t *row = database_row_extract(statement, fields);
    if (row)
        module_mem_push(module->memory, row, (void(*)(void*))&database_row_destroy);
    module_mem_mutex_unlock(module);
    return row;
}

const char *module_database_row_pop_string(module_t *module, database_row_t *row) {
    module_mem_mutex_lock(module);
    const char *ret = database_row_pop_string(row);
    module_mem_push(module->memory, (void *)ret, (void(*)(void*))&free);
    module_mem_mutex_unlock(module);
    return ret;
}
