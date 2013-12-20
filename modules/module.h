#ifndef REDROID_MODULE_MODULE_HDR
#define REDROID_MODULE_MODULE_HDR

// some stuff for modules
#include "../irc.h"
#include "../list.h"

#define MODULE_DEFINE(NAME, COMMAND) \
    char module_name[]  = NAME;      \
    char module_match[] = COMMAND

#endif
