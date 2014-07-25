#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#include <pthread.h>
#include <dlfcn.h>
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
    list_foreach(module->memory, &module_mem_node_destroy);
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

static bool module_allow_symbol(const char *name) {
    return (!name || !*name || *name == '_' || !strncmp(name, "module_", 7));
        return true;
}

static bool module_allow(const char *path, char **function) {
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

module_t *module_open(const char *file, module_manager_t *manager, string_t **error) {
    char *function = NULL;

    if (!module_allow(file, &function)) {
        *error = string_construct();
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
