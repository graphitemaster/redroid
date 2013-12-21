#ifndef REDROID_MODULE_HDR
#define REDROID_MODULE_HDR
#include "irc.h"

struct module_s;
typedef struct module_s module_t;

struct module_s {
    void       *handle;
    const char *name;
    const char *match;
    char       *file;
    void      (*enter)(irc_t *irc, const char *channel, const char *user, const char *message);
    void      (*close)(irc_t *irc);
    irc_t      *instance;
};

module_t *module_open(const char *file, irc_t *instance);
void module_destroy(module_t *module);
bool module_reload(module_t *module);

#endif
