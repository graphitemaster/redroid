#ifndef REDROID_MODULE_HDR
#define REDROID_MODULE_HDR
#include "irc.h"
typedef struct module_s module_t;

module_t *module_open(const char *file, irc_t *instance);
void module_close(module_t *module);
const char *module_file(module_t *module);
void module_enter(module_t *module);
const char *module_name(module_t *module);

#endif
