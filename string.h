#ifndef REDROID_STRING_HDR
#define REDROID_STRING_HDR
#include <stddef.h>
#include <stdbool.h>

struct string_s;
typedef struct string_s string_t;

struct string_s {
    char   *buffer;
    size_t  allocated;
    size_t  length;
};

void string_catf(string_t *string, const char *fmt, ...);
string_t *string_construct(void);
string_t *string_create(const char *contents);
void string_destroy(string_t *string);
size_t string_length(string_t *string);
bool string_empty(string_t *string);
const char *const string_contents(string_t *string);

#endif
