#ifndef REDROID_INI_HDR
#define REDROID_INI_HDR
#include <stdbool.h>

#define INI_MAX_SECT 2048
#define INI_MAX_NAME 2048
#define INI_MAX_LINE 4096

typedef bool (*ini_callback_t)(void *, const char *, const char *, const char *);
bool ini_parse(const char *file, ini_callback_t cb, void *user);

#endif
