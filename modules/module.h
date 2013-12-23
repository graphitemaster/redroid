#ifndef REDROID_MODULE_MODULE_HDR
#define REDROID_MODULE_MODULE_HDR

// some stuff for modules
#include "../irc.h"
#include "../list.h"
#include "../string.h"
#include "../module.h"

// only runs when command match
#define MODULE_DEFAULT(NAME)      \
    char module_name[]  = #NAME;  \
    char module_match[] = #NAME

// always runs
#define MODULE_ALWAYS(NAME)       \
    char module_name[]  = #NAME;  \
    char module_match[] = ""

#endif
