#ifndef __MATH
#define __MATH

#define __MODULE_LIBC_CALL(NAME) \
    ({ extern __typeof__(NAME) module_libc_##NAME; &module_libc_##NAME; })

#define NAN nan("")

static inline double pow(double x, double y) {
    return __MODULE_LIBC_CALL(pow)(x, y);
}

static inline int isnan(double value) {
    return __MODULE_LIBC_CALL(isnan)(value);
}

static inline int isinf(double value) {
    return __MODULE_LIBC_CALL(isinf)(value);
}

static inline double floor(double x) {
    return __MODULE_LIBC_CALL(floor)(x);
}

static inline double ceil(double x) {
    return __MODULE_LIBC_CALL(ceil)(x);
}

static inline double trunc(double x) {
    return __MODULE_LIBC_CALL(trunc)(x);
}

static inline double sqrt(double x) {
    return __MODULE_LIBC_CALL(sqrt)(x);
}

static inline double sin(double x) {
    return __MODULE_LIBC_CALL(sin)(x);
}

static inline double cos(double x) {
    return __MODULE_LIBC_CALL(cos)(x);
}

static inline double tan(double x) {
    return __MODULE_LIBC_CALL(tan)(x);
}

static inline double asin(double x) {
    return __MODULE_LIBC_CALL(asin)(x);
}

static inline double acos(double x) {
    return __MODULE_LIBC_CALL(acos)(x);
}

static inline double atan(double x) {
    return __MODULE_LIBC_CALL(atan)(x);
}

static inline double nan(const char *tag) {
    return __MODULE_LIBC_CALL(nan)(tag);
}

static inline double sinh(double x) {
    return __MODULE_LIBC_CALL(sinh)(x);
}

static inline double cosh(double x) {
    return __MODULE_LIBC_CALL(cosh)(x);
}

static inline double tanh(double x) {
    return __MODULE_LIBC_CALL(tanh)(x);
}

static inline double exp(double x) {
    return __MODULE_LIBC_CALL(exp)(x);
}

static inline double fabs(double x) {
    return __MODULE_LIBC_CALL(fabs)(x);
}

static inline double log(double x) {
    return __MODULE_LIBC_CALL(log)(x);
}

#undef __MODULE_LIBC_CALL
#endif
