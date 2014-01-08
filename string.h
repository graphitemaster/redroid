#ifndef REDROID_STRING_HDR
#define REDROID_STRING_HDR
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

typedef struct string_s string_t;

void string_vcatf(string_t *string, const char *fmt, va_list varg);
void string_catf(string_t *string, const char *fmt, ...);
string_t *string_construct(void);
string_t *string_create(const char *contents);
void string_clear(string_t *string);
void string_destroy(string_t *string);
size_t string_length(string_t *string);
bool string_empty(string_t *string);
char *const string_contents(string_t *string);
char *string_move(string_t *string);
char *string_end(string_t *string);

#endif
