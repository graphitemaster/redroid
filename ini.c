#include <string.h>
#include <stdio.h>

#include <strings.h>

#include "ini.h"

#define INI_MAX_SIZE 2048
#define INI_MAX_LINE 4096

#define tolower(a) ((a)|0x20)
#define isspace(a) ({ int c = (a); !!((c >= '\t' && c <= '\r') || c == ' '); })

static char *ini_rstrip(char *s) {
    char *p = s + strlen(s);
    while (p > s && isspace(*--p))
        *p = '\0';
    return s;
}

static char *ini_lskip(char *s) {
    while (*s && isspace(*s)) s++;
    return s;
}

static char *ini_find(char *s, char c) {
    bool w = 0;
    while (*s && *s != c && !(w && *s == ';'))
        w = isspace(*s), s++;
    return s;
}

static char *ini_strncpy(char *dest, const char *src, size_t size) {
    strncpy(dest, src, size);
    dest[size - 1] = '\0';
    return dest;
}

size_t ini_parse_file(FILE* file, ini_callback_t handler, void *user) {
    char line[INI_MAX_LINE];
    char sect[INI_MAX_SIZE] = "";
    char prev[INI_MAX_SIZE] = "";

    char *start;
    char *end;
    char *name;
    char *value;
    size_t lineno = 0;
    size_t error  = 0;

    while (fgets(line, INI_MAX_LINE, file)) {
        lineno++;
        start = line;
        start = ini_lskip(ini_rstrip(start));

        if (strchr(";#", *start))
            ;
        else if (*start == '[') {
            if (*(end = ini_find(start + 1, ']')) == ']') {
                *end = '\0';
                ini_strncpy(sect, start + 1, sizeof(sect));
                *prev = '\0';
            } else if (!error)
                error = lineno;
        } else if (*start && *start != ';') {
            if (*(end = ini_find(start, '=')) != '=')
                end = ini_find(start, ':');
            if (strchr("=:", *end)) {
                *end = '\0';
                name = ini_rstrip(start);
                value = ini_lskip(end + 1);
                if (*(end = ini_find(value, '\0')) == ';')
                    *end = '\0';
                ini_rstrip(value);
                ini_strncpy(prev, name, sizeof(prev));
                if (!handler(user, sect, name, value) && !error)
                    error = lineno;
            } else if (!error)
                error = lineno;
        }
    }
    return error;
}

bool ini_parse(const char *filename, ini_callback_t handler, void *user) {
    FILE *file;
    if (!(file = fopen(filename, "r")))
        return false;

    size_t error = ini_parse_file(file, handler, user);
    fclose(file);
    return error == 0;
}

bool ini_boolean(const char *text) {
    if (!text)
        return false;

    if (!strcasecmp(text, "true"))  return true;
    if (!strcasecmp(text, "false")) return false;
    if (!strcasecmp(text, "yes"))   return true;
    if (!strcasecmp(text, "no"))    return false;

    if (*text == '0') return false;
    if (*text == '1') return true;

    return false;
}
