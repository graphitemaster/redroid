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
string_t *string_format(const char *fmt, ...);
string_t *string_vformat(const char *fmt, va_list va);
void string_clear(string_t *string);
void string_destroy(string_t *string);
size_t string_length(string_t *string);
bool string_empty(string_t *string);
char *string_contents(string_t *string);
char *string_move(string_t *string);
char *string_end(string_t *string);
void string_replace(string_t *string, const char *substr, const char *replace);
void string_reassociate(string_t *oldstr, string_t *newstr);
void string_shrink(string_t *string, size_t by);
#endif
