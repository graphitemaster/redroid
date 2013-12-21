#ifndef REDROID_MODULE_MODULE_HDR
#define REDROID_MODULE_MODULE_HDR

// some stuff for modules
#include "../irc.h"
#include "../list.h"
#include "../module.h"

#define MODULE_DEFAULT(NAME)      \
    char module_name[]  = #NAME;  \
    char module_match[] = #NAME

#endif
