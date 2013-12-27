#ifndef REDROID_MODULE_MODULE_HDR
#define REDROID_MODULE_MODULE_HDR
#include "../module.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define MODULE_DEFAULT(NAME) char module_name[]  = #NAME, module_match[] = #NAME
#define MODULE_ALWAYS(NAME)  char module_name[]  = #NAME, module_match[] = ""

void *module_malloc(module_t *module, size_t bytes);
string_t *module_string_create(module_t *module, const char *input);
string_t *module_string_construct(module_t *module);
list_iterator_t *module_list_iterator_create(module_t *module, list_t *list);
list_t *module_list_create(module_t *module);
int module_getaddrinfo(module_t *module, const char *mode, const char *service, const struct addrinfo *hints, struct addrinfo **result);

#define string_create        module_string_create
#define string_construct     module_string_construct
#define list_iterator_create module_list_iterator_create
#define list_create          module_list_create
#define getaddrinfo          module_getaddrinfo

#endif
