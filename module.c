#include "module.h"
#include "irc.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
#if INPTR_MAX == INT64_MAX
#   define ELF(X) Elf64_##X
#   define FUN(X) ELF64_ST_TYPE(X)
#else
#   define ELF(X) Elf32_##X
#   define FUN(X) ELF32_ST_TYPE(X)
#endif

static bool module_allow_symbol(const char *name) {
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

        // string.h
        "strcpy",     "strncpy",  "strcat",    "strncat",    "strxfrm",
        "strlen",     "strcmp",   "strncmp",   "strcoll",    "strchr",
        "strrchr",    "strspn",   "strcspn",   "strpbrk",    "strstr",
        "strtok",     "memset",   "memcpy",    "memmove",    "memcmp",
        "memchr",

        // time.h
        "difftime",   "time",     "clock",     "asctime",    "ctime",
        "strftime",   "gmtime",   "localtime", "mktime",

        // module.h
        "module_enter",
        "module_close",

        // to be removed:
        "getaddrinfo", "freeaddrinfo", "malloc", "free", "strdup",
        "asprintf",
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

static bool module_allow(const char *path) {
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
        if (FUN(dsymtab->st_info) == STT_FUNC) {
            if (!module_allow_symbol(&dstrtab[dsymtab->st_name])) {
                fprintf(stderr, "%s", &dstrtab[dsymtab->st_name]);
                free(base);
                return false;
            }
        }
        dsymtab++;
    }

    free(base);
    return true;
}

module_t *module_open(const char *file, irc_t *instance) {
    if (!module_allow(file))
        return NULL;

    module_t *module = malloc(sizeof(*module));

    if (!(module->handle = dlopen(file, RTLD_LAZY))) {
        free(module);
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
