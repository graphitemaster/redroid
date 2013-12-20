#include "module.h"
#include "irc.h"

#include <dlfcn.h>  // dlopen, dlclose, dlsym, RTLD_LAZY
#include <stdlib.h> // malloc, free
#include <stdio.h>  // fprintf, stderr
struct module_s {
    void       *handle;
    const char *name;
    const char *match;
    char       *file;
    void      (*enter)(irc_t *irc);
    void      (*close)(irc_t *irc);
    irc_t      *instance;
};

module_t *module_open(const char *file, irc_t *instance) {
    module_t *module = malloc(sizeof(*module));

    if (!(module->handle = dlopen(file, RTLD_LAZY)))
        goto module_open_error;

    module->file     = strdup(file);
    module->instance = instance;

    //
    // POSIX.1-2003 (Technical Corrigendum 1) see Rational for how this
    // is legal despite it being 'illegal' in C99, which leaves casting
    // from "void *" to a function pointer undefined.
    //
    *(void **)(&module->enter) = dlsym(module->handle, "module_enter");
    *(void **)(&module->close) = dlsym(module->handle, "module_close");

    if (!(module->name = (const char *)dlsym(module->handle, "module_name"))) {
        fprintf(stderr, "missing module name in `%s`\n", module->file);
        goto module_open_error;
    }

#   define MODULE_TRY(THING, NAME, DESC) \
        do { \
            if (!(module->THING = dlsym(module->handle, NAME))) { \
                fprintf(stderr, "missing %s in %s => %s\n", DESC, module->name, module->file); \
                goto module_open_error; \
            } \
        } while (0) \

    MODULE_TRY(match, "module_match", "command match");
    MODULE_TRY(enter, "module_enter", "entry function");
    MODULE_TRY(close, "module_close", "cleanup function");

#   undef MODULE_TRY

    return module;

module_open_error:
    if (module->handle)
        dlclose(module->handle);
    free(module);
    return NULL;
}

void module_enter(module_t *module) {
    module->enter(module->instance);
}

void module_close(module_t *module) {
    module->close(module->instance);
    dlclose(module->handle);
    free(module->file);
    free(module);
}

const char *module_file(module_t *module) {
    return module->file;
}

const char *module_name(module_t *module) {
    return module->name;
}
