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

#   define MODULE_TRY(THING, NAME, DESC) \
        do { \
            if (!(module->THING = dlsym(module->handle, NAME))) { \
                fprintf(stderr, "missing %s in %s => %s\n", DESC, module->name, module->file); \
                return NULL; \
            } \
        } while (0) \

    MODULE_TRY(match, "module_match", "command match");
    MODULE_TRY(enter, "module_enter", "entry function");
    MODULE_TRY(close, "module_close", "cleanup function");
#   undef MODULE_TRY

    return module;
}

module_t *module_open(const char *file, irc_t *instance) {
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
    module->close(module->instance);
    dlclose(module->handle);
    if (!(module->handle = dlopen(module->file, RTLD_LAZY)))
        goto module_reload_error;

    if (!module_load(module))
        goto module_reload_error;

    return true;

module_reload_error:
    free(module->file);
    free(module);
    return false;
}

void module_enter(module_t *module, const char *channel, const char *user, const char *message) {
    module->enter(module->instance, channel, user, message);
}

void module_destroy(module_t *module) {
    module->close(module->instance);
    dlclose(module->handle);
    free(module->file);
    free(module);
}
