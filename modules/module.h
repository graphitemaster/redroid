#ifndef REDROID_MODULE_MODULE_HDR
#define REDROID_MODULE_MODULE_HDR

// some stuff for modules
#include "../irc.h"
#include "../list.h"
#include "../string.h"
#include "../module.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define MODULE_DEFAULT(NAME) char module_name[]  = #NAME, module_match[] = #NAME
#define MODULE_ALWAYS(NAME)  char module_name[]  = #NAME, module_match[] = ""

#ifdef __cplusplus
extern "C" {
#endif

void *module_alloc(module_t *module, size_t bytes);
int module_getaddrinfo(module_t *module, const char *mode, const char *service, const struct addrinfo *hints, struct addrinfo **result);

#ifdef _cplusplus
}
#endif

#endif
