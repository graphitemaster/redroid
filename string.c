#include "string.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void string_reallocate(string_t *string) {
    size_t size   = string->allocated * 2;
    char  *buffer = malloc(size);

    strcpy(buffer, string->buffer);
    free(string->buffer);
    string->buffer    = buffer;
    string->allocated = size;
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

void string_destroy(string_t *string) {
    free(string->buffer);
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
