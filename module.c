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

static module_t *module_load(module_t *module) {
    //
    // POSIX.1-2003 (Technical Corrigendum 1) see Rational for how this
    // is legal despite it being 'illegal' in C99, which leaves casting
    // from "void *" to a function pointer undefined.
    //
    *(void **)(&module->enter) = dlsym(module->handle, "module_enter");
    *(void **)(&module->close) = dlsym(module->handle, "module_close");

    if (!(module->name = (const char *)dlsym(module->handle, "module_name"))) {
        fprintf(stderr, "missing module name in `%s`\n", module->file);
        return NULL;
    }

    if (!(module->match = dlsym(module->handle, "module_match"))) {
        fprintf(stderr, "   module  => missing command match rule %s [%s]\n", module->name, module->file);
        return NULL;
    }

    if (!(module->enter = dlsym(module->handle, "module_enter"))) {
        fprintf(stderr, "   module  => missing command handler %s [%s]\n", module->name, module->file);
        return NULL;
    }

    return module;
}

//
// ensure a module doesn't use any functions by the ones provided by
// a whitelist. This way we can ensure modules don't allocate resources
// with standard library functions.
//
#include <elf.h>
#include <stdint.h>
#if __x86_64__
#   define ELF(X) Elf64_##X
#   define FUN(X) ELF64_ST_TYPE(X)
#else
#   define ELF(X) Elf32_##X
#   define FUN(X) ELF32_ST_TYPE(X)
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

        // string.h
        "string_contents",
        "string_length",
        "string_catf",

        // irc.h
        "irc_modules_add",
        "irc_action",
        "irc_write",

        // misc
        "inet_ntop",

        // to be removed:
        "raise",
        "sqlite3_exec", "sqlite3_open", "sqlite3_prepare_v2",
        "sqlite3_column_text", "sqlite3_column_int", "sqlite3_errmsg",
        "sqlite3_step", "sqlite3_close", "sqlite3_bind_text",
        "sqlite3_finalize", "sqlite3_free"
    };

    for (size_t i = 0; i < sizeof(list) / sizeof(*list); i++)
        if (!strcmp(list[i], name) || *name == '_')
            return true;

    return false;
}

static bool module_allow(const char *path, char **function) {
    void      *base;
    size_t     size;
    ELF(Ehdr) *ehdr;
    ELF(Shdr) *shdr;
    ELF(Sym)  *dsymtab;
    ELF(Sym)  *dsymtab_end;
    char      *dstrtab;
    FILE      *file = fopen(path, "r");

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    base = malloc(size);
    fseek(file, 0, SEEK_SET);
    fread(base, size, 1, file);
    fclose(file);

    ehdr = base;
    shdr = (ELF(Shdr)*)(base + ehdr->e_shoff);
    for (size_t i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_DYNSYM) {
            dsymtab     = (ELF(Sym)*)(base + shdr[i].sh_offset);
            dsymtab_end = (ELF(Sym)*)((char *)dsymtab + shdr[i].sh_size);
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

    if (!(module = module_load(module))) {
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

//
// module API wrappers around things which allocate memory.
// these register allocations with the garbage collector.
//
#define MODULE_MEM_SCOPE_BEG(MOD) \
    module_mem_mutex_lock(MOD)

#define MODULE_MEM_SCOPE_END(MOD, FREE, ITEM)                            \
    do {                                                                 \
        module_mem_push((MOD)->memory, (ITEM), (void(*)(void*))&(FREE)); \
        module_mem_mutex_unlock(MOD);                                    \
    } while (0)

void *module_malloc(module_t *module, size_t bytes) {
    MODULE_MEM_SCOPE_BEG(module);
    void *p = malloc(bytes);
    MODULE_MEM_SCOPE_END(module, p, free);
    return p;
}

string_t *module_string_create(module_t *module, const char *input) {
    MODULE_MEM_SCOPE_BEG(module);
    string_t *string = string_create(input);
    MODULE_MEM_SCOPE_END(module, string, string_destroy);
    return string;
}

string_t *module_string_construct(module_t *module) {
    MODULE_MEM_SCOPE_BEG(module);
    string_t *string = string_construct();
    MODULE_MEM_SCOPE_END(module, string, string_destroy);
    return string;
}

list_iterator_t *module_list_iterator_create(module_t *module, list_t *list) {
    MODULE_MEM_SCOPE_BEG(module);
    list_iterator_t *it = list_iterator_create(list);
    MODULE_MEM_SCOPE_END(module, it, list_iterator_destroy);
    return it;
}

list_t *module_list_create(module_t *module) {
    MODULE_MEM_SCOPE_BEG(module);
    list_t *list = list_create();
    MODULE_MEM_SCOPE_END(module, list, list_destroy);
    return list;
}

int module_getaddrinfo(module_t *module, const char *mode, const char *service, const struct addrinfo *hints, struct addrinfo **result) {
    MODULE_MEM_SCOPE_BEG(module);
    int value = getaddrinfo(mode, service, hints, result);
    if (value == 0)
        MODULE_MEM_SCOPE_END(module, *result, freeaddrinfo);
    return value;
}
