#ifndef REDROID_INI_HDR
#define REDROID_INI_HDR
#include <stdbool.h>

typedef bool (*ini_callback_t)(void *, const char *, const char *, const char *);
bool ini_parse(const char *file, ini_callback_t cb, void *user);

bool ini_boolean(const char *text);

#endif
