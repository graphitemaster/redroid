#ifndef __STDLIB
#define __STDLIB
#define __MODULE_LIBC_CALL(NAME) \
    ({ extern __typeof__(NAME) module_libc_##NAME; &module_libc_##NAME; })

static inline double strtod(const char *str, char **end) {
    return __MODULE_LIBC_CALL(strtod)(str, end);
}

static inline int atoi(const char *str) {
    return __MODULE_LIBC_CALL(atoi)(str);
}

static inline unsigned long long strtoull(const char *str, char **end, int base) {
    return __MODULE_LIBC_CALL(strtoull)(str, end, base);
}

#undef __MODULE_LIBC_CALL
#endif
