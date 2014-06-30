#ifndef __STRING
#define __STRING
#define __MODULE_LIBC_CALL(NAME) \
    ({ extern __typeof__(NAME) module_libc_##NAME; &module_libc_##NAME; })

static inline void *memset(void *dest, int ch, size_t n) {
    return __MODULE_LIBC_CALL(memset)(dest, ch, n);
}

static inline void *malloc(size_t size) {
    return __MODULE_LIBC_CALL(malloc)(size);
}

static inline void *memcpy(void *dest, const void *src, size_t n) {
    return __MODULE_LIBC_CALL(memcpy)(dest, src, n);
}

static inline int memcmp(const void *s1, const void *s2, size_t n) {
    return __MODULE_LIBC_CALL(memcmp)(s1, s2, n);
}

static inline size_t strlen(const char *src) {
    return __MODULE_LIBC_CALL(strlen)(src);
}

static inline char *strchr(const char *src, int ch) {
    return __MODULE_LIBC_CALL(strchr)(src, ch);
}

static inline int strcmp(const char *s1, const char *s2) {
    return __MODULE_LIBC_CALL(strcmp)(s1, s2);
}

static inline int strcasecmp(const char *s1, const char *s2) {
    return __MODULE_LIBC_CALL(strcasecmp)(s1, s2);
}

static inline char *strstr(const char *haystack, const char *needle) {
    return __MODULE_LIBC_CALL(strstr)(haystack, needle);
}

static inline char *strdup(const char *src) {
    return __MODULE_LIBC_CALL(strdup)(src);
}

#undef __MODULE_LIBC_CALL
#endif
