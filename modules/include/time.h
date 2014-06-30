#ifndef __TIME
#define __TIME
#define __MODULE_LIBC_CALL(NAME) \
    ({ extern __typeof__(NAME) module_libc_##NAME; &module_libc_##NAME; })

typedef long time_t;

static inline time_t time(time_t *timer) {
    return __MODULE_LIBC_CALL(time)(timer);
}

#undef __MODULE_LIBC_CALL
#endif
