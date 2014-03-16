#include "mt.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MT_N 624
#define MT_M 397

struct mt_s {
    uint32_t  state[MT_N];
    uint32_t *next;
    size_t    remaining;
};

enum {
    UMASK = 0x80000000U,
    LMASK = 0x7FFFFFFFU,
    FMASK = 0xFFFFFFFFU,
    MMASK = 0x9908B0DFU
};

#define MBITS(U, V) (((U) & UMASK) | ((V) & LMASK))
#define TWIST(U, V) ((MBITS(U, V) >> 1) ^ ((V) & 1U ? MMASK : 0U))

static unsigned long mt_seed(void) {
    uint32_t a = (uint32_t)clock();
    uint32_t b = (uint32_t)time(0);
    uint32_t c = (uint32_t)getpid();

    #define r(x, k) (((x)<<(k)) | ((x)>>(32-(k))))

    a -= c; a ^= r(c,  4); c += b;
    b -= a; b ^= r(a,  6); a += c;
    c -= b; c ^= r(b,  8); b += a;
    a -= c; a ^= r(c, 16); c += b;
    b -= a; b ^= r(a, 19);
    c -= b; c ^= r(b,  4);

    #undef r
    return (unsigned long)c;
}

static mt_t *mt_init(mt_t *mt, unsigned long seed) {
    *mt->state = (uint32_t)(seed & FMASK);
    for (size_t i = 1; i < MT_N; i++) {
        mt->state[i] = (1812433253U * (mt->state[i - 1] ^ (mt->state[i - 1] >> 30)) + i);
        mt->state[i] &= FMASK;
    }
    mt->remaining = 0;
    mt->next      = NULL;
    return mt;
}

static void mt_update(mt_t *mt) {
    uint32_t *p = mt->state;
    for (size_t i = (MT_N - MT_M + 1); --i; p++)
        *p = p[MT_M] ^ TWIST(p[0], p[1]);

    for (size_t i = MT_M; --i; p++)
        *p = p[MT_M - MT_N] ^ TWIST(p[0], p[1]);
    *p = p[MT_M - MT_N] ^ TWIST(p[0], *mt->state);

    mt->remaining = MT_N;
    mt->next      = mt->state;
}


mt_t *mt_create(void) {
    mt_t *mt = memset(malloc(sizeof(mt_t)), 0, sizeof(mt_t));
    return mt_init(mt, mt_seed());
}

void mt_destroy(mt_t *mt) {
    free(mt);
}

uint32_t mt_urand(mt_t *mt) {
    uint32_t r;
    if (!mt->remaining)
        mt_update(mt);

    r = *mt->next++;
    mt->remaining--;

    /* standard tempering */
    r ^= (r >> 11);
    r ^= (r << 7)  & 0x9D2C5680U;
    r ^= (r << 15) & 0xEFC60000U;
    r ^= (r >> 18);
    r &= FMASK;

    return r;
}

double mt_drand(mt_t *mt) {
    return mt_urand(mt) / 4294967296.0; /* 2^32 */
}
