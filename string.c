#include "string.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct string_s {
    char   *buffer;
    size_t  allocated;
    size_t  length;
};

static void string_reallocate(string_t *string) {
    size_t size   = string->allocated * 2;
    char  *buffer = malloc(size);

    strcpy(buffer, string->buffer);
    free(string->buffer);
    string->buffer    = buffer;
    string->allocated = size;
}

static void string_reassociate(string_t *oldstr, string_t *newstr) {
    size_t allocated = newstr->allocated;
    size_t length    = newstr->length;
    char  *move      = string_move(newstr);

    string_clear(oldstr);

    oldstr->buffer    = move;
    oldstr->allocated = allocated;
    oldstr->length    = length;

    free(newstr);
}

void string_vcatf(string_t *string, const char *fmt, va_list varg) {
    va_list va;
    for (;;) {
        size_t left = string->allocated - string->length;
        size_t write;

        va_copy(va, varg);
        write = vsnprintf(string->buffer + string->length, left, fmt, va);
        va_end(va);

        if (left <= write) {
            string_reallocate(string);
            continue;
        }

        string->length += write;
        return;
    }
}

void string_catf(string_t *string, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    string_vcatf(string, fmt, va);
    va_end(va);
}

string_t *string_construct(void) {
    string_t *string = memcpy(malloc(sizeof(string_t)),
        &(string_t) {
            .buffer    = malloc(8),
            .allocated = 8,
            .length    = 0
        },
        sizeof(string_t)
    );
    *string->buffer = '\0';
    return string;
}

string_t *string_create(const char *contents) {
    return memcpy(malloc(sizeof(string_t)),
        &(string_t) {
            .buffer    = strdup(contents),
            .allocated = strlen(contents),
            .length    = strlen(contents)
        },
        sizeof(string_t)
    );
}

string_t *string_vformat(const char *fmt, va_list va) {
    string_t *string = string_construct();
    string_vcatf(string, fmt, va);
    return string;
}

string_t *string_format(const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    string_t *string = string_vformat(fmt, va);
    va_end(va);
    return string;
}

void string_clear(string_t *string) {
    free(string->buffer);
    string->buffer    = NULL;
    string->allocated = 0;
    string->length    = 0;
}

void string_destroy(string_t *string) {
    string_clear(string);
    free(string);
}

char *const string_contents(string_t *string) {
    return (string) ? string->buffer : NULL;
}

size_t string_length(string_t *string) {
    return (string) ? string->length : 0;
}

bool string_empty(string_t *string) {
    return (string_length(string) == 0) || !*string->buffer;
}

char *string_move(string_t *string) {
    char *data = string->buffer;
    string->buffer = NULL;
    string_clear(string);
    return data;
}

char *string_end(string_t *string) {
    char *data = string_move(string);
    string_destroy(string);
    return data;
}

void string_replace(string_t *string, const char *search, const char *replace) {
    string_t *modified = NULL;
    char     *content  = string_contents(string);
    char     *find     = strstr(content, search);

    if (!find)
        return;

    modified = string_construct();
    while (find) {
        *find = '\0';
        if (replace)
            string_catf(modified, "%s%s", content, replace);
        else
            string_catf(modified, "%s", content);

        content = &find[strlen(search)];
        find    = strstr(content, search);
    }
    if (strlen(content))
        string_catf(modified, "%s", content);

    string_reassociate(string, modified);
}
