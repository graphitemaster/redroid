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
