/* Implementation of the C Standard Library for modules */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

#include "module.h"

/* stdlib.h */
double module_libc_strtod(const char *str, char **end) {
    return strtod(str, end);
}

int module_libc_atoi(const char *str) {
    return atoi(str);
}

unsigned long long module_libc_strtoull(const char *str, char **end, int base) {
    return strtoull(str, end, base);
}

/* math.h */
double module_libc_pow(double x, double y) {
    return pow(x, y);
}

int module_libc_isnan(double value) {
    return isnan(value);
}

int module_libc_isinf(double value) {
    return isinf(value);
}

double module_libc_floor(double x) {
    return floor(x);
}

double module_libc_ceil(double x) {
    return ceil(x);
}

double module_libc_trunc(double x) {
    return trunc(x);
}

double module_libc_sqrt(double x) {
    return sqrt(x);
}

double module_libc_sin(double x) {
    return sin(x);
}

double module_libc_cos(double x) {
    return cos(x);
}

double module_libc_tan(double x) {
    return tan(x);
}

double module_libc_asin(double x) {
    return asin(x);
}

double module_libc_acos(double x) {
    return acos(x);
}

double module_libc_atan(double x) {
    return atan(x);
}

double module_libc_nan(const char *tag) {
    return nan(tag);
}

double module_libc_sinh(double x) {
    return sinh(x);
}

double module_libc_cosh(double x) {
    return cosh(x);
}

double module_libc_tanh(double x) {
    return tanh(x);
}

double module_libc_exp(double x) {
    return exp(x);
}

double module_libc_fabs(double x) {
    return fabs(x);
}

double module_libc_log(double x) {
    return log(x);
}

/* string.h */
void *module_libc_memset(void *dest, int ch, size_t n) {
    return memset(dest, ch, n);
}

void *module_libc_malloc(size_t size) {
    module_t *module = module_singleton_get();
    void *memory = malloc(size);
    if (memory)
        module_mem_push(module, memory, &free);
    return memory;
}

void *module_libc_memcpy(void *dest, const void *src, size_t n) {
    return memcpy(dest, src, n);
}

int module_libc_memcmp(const void *s1, const void *s2, size_t n) {
    return memcmp(s1, s2, n);
}

size_t module_libc_strlen(const char *src) {
    return strlen(src);
}

char *module_libc_strchr(const char *src, int ch) {
    return strchr(src, ch);
}

int module_libc_strcmp(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

int module_libc_strcasecmp(const char *s1, const char *s2) {
    return strcasecmp(s1, s2);
}

char *module_libc_strstr(const char *haystack, const char *needle) {
    return strstr(haystack, needle);
}

char *module_libc_strdup(const char *src) {
    module_t *module = module_singleton_get();
    char *dest = strdup(src);
    if (dest)
        module_mem_push(module, dest, &free);
    return dest;
}

/* time.h */
time_t module_libc_time(time_t *timer) {
    return time(timer);
}
