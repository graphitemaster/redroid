#include "ini.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>


static char *rstrip(char* s) {
    char* p = s + strlen(s);
    while (p > s && isspace((unsigned char)(*--p)))
        *p = '\0';
    return s;
}


static char* lskip(const char* s) {
    while (*s && isspace((unsigned char)(*s)))
        s++;
    return (char*)s;
}

static char* find_char_or_comment(const char* s, char c) {
    int was_whitespace = 0;
    while (*s && *s != c && !(was_whitespace && *s == ';')) {
        was_whitespace = isspace((unsigned char)(*s));
        s++;
    }
    return (char*)s;
}

static char* strncpy0(char* dest, const char* src, size_t size) {
    strncpy(dest, src, size);
    dest[size - 1] = '\0';
    return dest;
}

size_t ini_parse_file(FILE* file, ini_callback_t handler, void* user) {
    char line[INI_MAX_LINE];
    char section[INI_MAX_SECT] = "";
    char prev_name[INI_MAX_NAME] = "";

    char *start;
    char *end;
    char *name;
    char *value;
    size_t lineno = 0;
    size_t error  = 0;

    while (fgets(line, INI_MAX_LINE, file) != NULL) {
        lineno++;
        start = line;
        start = lskip(rstrip(start));

        if (*start == ';' || *start == '#') {

        }
        else if (*start == '[') {
            end = find_char_or_comment(start + 1, ']');
            if (*end == ']') {
                *end = '\0';
                strncpy0(section, start + 1, sizeof(section));
                *prev_name = '\0';
            }
            else if (!error) {
                error = lineno;
            }
        }
        else if (*start && *start != ';') {
            end = find_char_or_comment(start, '=');
            if (*end != '=') {
                end = find_char_or_comment(start, ':');
            }
            if (*end == '=' || *end == ':') {
                *end = '\0';
                name = rstrip(start);
                value = lskip(end + 1);
                end = find_char_or_comment(value, '\0');
                if (*end == ';')
                    *end = '\0';
                rstrip(value);
                strncpy0(prev_name, name, sizeof(prev_name));
                if (!handler(user, section, name, value) && !error)
                    error = lineno;
            }
            else if (!error) {
                error = lineno;
            }
        }
    }
    return error;
}

bool ini_parse(const char* filename, ini_callback_t handler, void *user) {
    FILE *file;
    int   error;

    file = fopen(filename, "r");
    if (!file)
        return -1;

    error = ini_parse_file(file, handler, user);
    fclose(file);
    return !!(error == 0);
}


bool ini_boolean(const char *text) {
    if (!text)
        return false;

    if (!strcasecmp(text, "true"))  return true;
    if (!strcasecmp(text, "false")) return false;

    if (*text == '0') return false;
    if (*text == '1') return true;

    return false;
}
