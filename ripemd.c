#include <string.h>
#include <stdlib.h>

#include "ripemd.h"

#define BYTES_TO_DWORD(strptr)         \
    (((uint32_t)*((strptr)+3) << 24) | \
     ((uint32_t)*((strptr)+2) << 16) | \
     ((uint32_t)*((strptr)+1) <<  8) | \
     ((uint32_t)*((strptr)+0)))

#define ROL(x, n) (((x) << (n)) | ((x) >> (32-(n))))

#define F(x, y, z) ((x) ^ (y) ^ (z))
#define G(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define H(x, y, z) (((x) | ~(y)) ^ (z))
#define I(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define J(x, y, z) ((x) ^ ((y) | ~(z)))

#define FF(a, b, c, d, e, x, s)                       \
    do {                                              \
        (a) += F((b), (c), (d)) + (x);                \
        (a)  = ROL((a), (s)) + (e);                   \
        (c)  = ROL((c), 10);                          \
    } while (0)

#define GG(a, b, c, d, e, x, s)                       \
    do {                                              \
        (a) += G((b), (c), (d)) + (x) + 0x5a827999UL; \
        (a)  = ROL((a), (s)) + (e);                   \
        (c)  = ROL((c), 10);                          \
    } while (0)

#define HH(a, b, c, d, e, x, s)                       \
    do {                                              \
        (a) += H((b), (c), (d)) + (x) + 0x6ed9eba1UL; \
        (a)  = ROL((a), (s)) + (e);                   \
        (c)  = ROL((c), 10);                          \
    } while (0)

#define II(a, b, c, d, e, x, s)                       \
    do {                                              \
        (a) += I((b), (c), (d)) + (x) + 0x8f1bbcdcUL; \
        (a)  = ROL((a), (s)) + (e);                   \
        (c)  = ROL((c), 10);                          \
    } while (0)

#define JJ(a, b, c, d, e, x, s)                       \
    do {                                              \
        (a) += J((b), (c), (d)) + (x) + 0xa953fd4eUL; \
        (a)  = ROL((a), (s)) + (e);                   \
        (c)  = ROL((c), 10);                          \
    } while (0)

#define FFF(a, b, c, d, e, x, s)                      \
    do {                                              \
        (a) += F((b), (c), (d)) + (x);                \
        (a)  = ROL((a), (s)) + (e);                   \
        (c)  = ROL((c), 10);                          \
    } while (0)

#define GGG(a, b, c, d, e, x, s)                      \
    do {                                              \
        (a) += G((b), (c), (d)) + (x) + 0x7a6d76e9UL; \
        (a)  = ROL((a), (s)) + (e);                   \
        (c)  = ROL((c), 10);                          \
    } while (0)

#define HHH(a, b, c, d, e, x, s)                      \
    do {                                              \
        (a) += H((b), (c), (d)) + (x) + 0x6d703ef3UL; \
        (a)  = ROL((a), (s)) + (e);                   \
        (c)  = ROL((c), 10);                          \
    } while (0)

#define III(a, b, c, d, e, x, s)                      \
    do {                                              \
        (a) += I((b), (c), (d)) + (x) + 0x5c4dd124UL; \
        (a)  = ROL((a), (s)) + (e);                   \
        (c)  = ROL((c), 10);                          \
    } while (0)

#define JJJ(a, b, c, d, e, x, s)                      \
    do {                                              \
        (a) += J((b), (c), (d)) + (x) + 0x50a28be6UL; \
        (a)  = ROL((a), (s)) + (e);                   \
        (c)  = ROL((c), 10);                          \
    } while (0)

struct ripemd_s {
    uint32_t buffer[4];
};

void ripemd_reset(ripemd_t *rmd) {
    rmd->buffer[0] = 0x67452301UL;
    rmd->buffer[1] = 0xefcdab89UL;
    rmd->buffer[2] = 0x98badcfeUL;
    rmd->buffer[3] = 0x10325476UL;
    rmd->buffer[4] = 0xc3d2e1f0UL;
}

ripemd_t *ripemd_create(void) {
    ripemd_t *rmd = malloc(sizeof(*rmd));
    ripemd_reset(rmd);
    return rmd;
}

void ripemd_destroy(ripemd_t *rmd) {
    free(rmd);
}

void ripemd_compress(ripemd_t *rmd, uint32_t *X) {
    uint32_t aa  = rmd->buffer[0];
    uint32_t bb  = rmd->buffer[1];
    uint32_t cc  = rmd->buffer[2];
    uint32_t dd  = rmd->buffer[3];
    uint32_t ee  = rmd->buffer[4];
    uint32_t aaa = rmd->buffer[0];
    uint32_t bbb = rmd->buffer[1];
    uint32_t ccc = rmd->buffer[2];
    uint32_t ddd = rmd->buffer[3];
    uint32_t eee = rmd->buffer[4];

    /* standard rounds */
    FF(aa, bb, cc, dd, ee, X[ 0], 11);
    FF(ee, aa, bb, cc, dd, X[ 1], 14);
    FF(dd, ee, aa, bb, cc, X[ 2], 15);
    FF(cc, dd, ee, aa, bb, X[ 3], 12);
    FF(bb, cc, dd, ee, aa, X[ 4],  5);
    FF(aa, bb, cc, dd, ee, X[ 5],  8);
    FF(ee, aa, bb, cc, dd, X[ 6],  7);
    FF(dd, ee, aa, bb, cc, X[ 7],  9);
    FF(cc, dd, ee, aa, bb, X[ 8], 11);
    FF(bb, cc, dd, ee, aa, X[ 9], 13);
    FF(aa, bb, cc, dd, ee, X[10], 14);
    FF(ee, aa, bb, cc, dd, X[11], 15);
    FF(dd, ee, aa, bb, cc, X[12],  6);
    FF(cc, dd, ee, aa, bb, X[13],  7);
    FF(bb, cc, dd, ee, aa, X[14],  9);
    FF(aa, bb, cc, dd, ee, X[15],  8);
    GG(ee, aa, bb, cc, dd, X[ 7],  7);
    GG(dd, ee, aa, bb, cc, X[ 4],  6);
    GG(cc, dd, ee, aa, bb, X[13],  8);
    GG(bb, cc, dd, ee, aa, X[ 1], 13);
    GG(aa, bb, cc, dd, ee, X[10], 11);
    GG(ee, aa, bb, cc, dd, X[ 6],  9);
    GG(dd, ee, aa, bb, cc, X[15],  7);
    GG(cc, dd, ee, aa, bb, X[ 3], 15);
    GG(bb, cc, dd, ee, aa, X[12],  7);
    GG(aa, bb, cc, dd, ee, X[ 0], 12);
    GG(ee, aa, bb, cc, dd, X[ 9], 15);
    GG(dd, ee, aa, bb, cc, X[ 5],  9);
    GG(cc, dd, ee, aa, bb, X[ 2], 11);
    GG(bb, cc, dd, ee, aa, X[14],  7);
    GG(aa, bb, cc, dd, ee, X[11], 13);
    GG(ee, aa, bb, cc, dd, X[ 8], 12);
    HH(dd, ee, aa, bb, cc, X[ 3], 11);
    HH(cc, dd, ee, aa, bb, X[10], 13);
    HH(bb, cc, dd, ee, aa, X[14],  6);
    HH(aa, bb, cc, dd, ee, X[ 4],  7);
    HH(ee, aa, bb, cc, dd, X[ 9], 14);
    HH(dd, ee, aa, bb, cc, X[15],  9);
    HH(cc, dd, ee, aa, bb, X[ 8], 13);
    HH(bb, cc, dd, ee, aa, X[ 1], 15);
    HH(aa, bb, cc, dd, ee, X[ 2], 14);
    HH(ee, aa, bb, cc, dd, X[ 7],  8);
    HH(dd, ee, aa, bb, cc, X[ 0], 13);
    HH(cc, dd, ee, aa, bb, X[ 6],  6);
    HH(bb, cc, dd, ee, aa, X[13],  5);
    HH(aa, bb, cc, dd, ee, X[11], 12);
    HH(ee, aa, bb, cc, dd, X[ 5],  7);
    HH(dd, ee, aa, bb, cc, X[12],  5);
    II(cc, dd, ee, aa, bb, X[ 1], 11);
    II(bb, cc, dd, ee, aa, X[ 9], 12);
    II(aa, bb, cc, dd, ee, X[11], 14);
    II(ee, aa, bb, cc, dd, X[10], 15);
    II(dd, ee, aa, bb, cc, X[ 0], 14);
    II(cc, dd, ee, aa, bb, X[ 8], 15);
    II(bb, cc, dd, ee, aa, X[12],  9);
    II(aa, bb, cc, dd, ee, X[ 4],  8);
    II(ee, aa, bb, cc, dd, X[13],  9);
    II(dd, ee, aa, bb, cc, X[ 3], 14);
    II(cc, dd, ee, aa, bb, X[ 7],  5);
    II(bb, cc, dd, ee, aa, X[15],  6);
    II(aa, bb, cc, dd, ee, X[14],  8);
    II(ee, aa, bb, cc, dd, X[ 5],  6);
    II(dd, ee, aa, bb, cc, X[ 6],  5);
    II(cc, dd, ee, aa, bb, X[ 2], 12);
    JJ(bb, cc, dd, ee, aa, X[ 4],  9);
    JJ(aa, bb, cc, dd, ee, X[ 0], 15);
    JJ(ee, aa, bb, cc, dd, X[ 5],  5);
    JJ(dd, ee, aa, bb, cc, X[ 9], 11);
    JJ(cc, dd, ee, aa, bb, X[ 7],  6);
    JJ(bb, cc, dd, ee, aa, X[12],  8);
    JJ(aa, bb, cc, dd, ee, X[ 2], 13);
    JJ(ee, aa, bb, cc, dd, X[10], 12);
    JJ(dd, ee, aa, bb, cc, X[14],  5);
    JJ(cc, dd, ee, aa, bb, X[ 1], 12);
    JJ(bb, cc, dd, ee, aa, X[ 3], 13);
    JJ(aa, bb, cc, dd, ee, X[ 8], 14);
    JJ(ee, aa, bb, cc, dd, X[11], 11);
    JJ(dd, ee, aa, bb, cc, X[ 6],  8);
    JJ(cc, dd, ee, aa, bb, X[15],  5);
    JJ(bb, cc, dd, ee, aa, X[13],  6);

    /* parallel rounds */
    JJJ(aaa, bbb, ccc, ddd, eee, X[ 5],  8);
    JJJ(eee, aaa, bbb, ccc, ddd, X[14],  9);
    JJJ(ddd, eee, aaa, bbb, ccc, X[ 7],  9);
    JJJ(ccc, ddd, eee, aaa, bbb, X[ 0], 11);
    JJJ(bbb, ccc, ddd, eee, aaa, X[ 9], 13);
    JJJ(aaa, bbb, ccc, ddd, eee, X[ 2], 15);
    JJJ(eee, aaa, bbb, ccc, ddd, X[11], 15);
    JJJ(ddd, eee, aaa, bbb, ccc, X[ 4],  5);
    JJJ(ccc, ddd, eee, aaa, bbb, X[13],  7);
    JJJ(bbb, ccc, ddd, eee, aaa, X[ 6],  7);
    JJJ(aaa, bbb, ccc, ddd, eee, X[15],  8);
    JJJ(eee, aaa, bbb, ccc, ddd, X[ 8], 11);
    JJJ(ddd, eee, aaa, bbb, ccc, X[ 1], 14);
    JJJ(ccc, ddd, eee, aaa, bbb, X[10], 14);
    JJJ(bbb, ccc, ddd, eee, aaa, X[ 3], 12);
    JJJ(aaa, bbb, ccc, ddd, eee, X[12],  6);
    III(eee, aaa, bbb, ccc, ddd, X[ 6],  9);
    III(ddd, eee, aaa, bbb, ccc, X[11], 13);
    III(ccc, ddd, eee, aaa, bbb, X[ 3], 15);
    III(bbb, ccc, ddd, eee, aaa, X[ 7],  7);
    III(aaa, bbb, ccc, ddd, eee, X[ 0], 12);
    III(eee, aaa, bbb, ccc, ddd, X[13],  8);
    III(ddd, eee, aaa, bbb, ccc, X[ 5],  9);
    III(ccc, ddd, eee, aaa, bbb, X[10], 11);
    III(bbb, ccc, ddd, eee, aaa, X[14],  7);
    III(aaa, bbb, ccc, ddd, eee, X[15],  7);
    III(eee, aaa, bbb, ccc, ddd, X[ 8], 12);
    III(ddd, eee, aaa, bbb, ccc, X[12],  7);
    III(ccc, ddd, eee, aaa, bbb, X[ 4],  6);
    III(bbb, ccc, ddd, eee, aaa, X[ 9], 15);
    III(aaa, bbb, ccc, ddd, eee, X[ 1], 13);
    III(eee, aaa, bbb, ccc, ddd, X[ 2], 11);
    HHH(ddd, eee, aaa, bbb, ccc, X[15],  9);
    HHH(ccc, ddd, eee, aaa, bbb, X[ 5],  7);
    HHH(bbb, ccc, ddd, eee, aaa, X[ 1], 15);
    HHH(aaa, bbb, ccc, ddd, eee, X[ 3], 11);
    HHH(eee, aaa, bbb, ccc, ddd, X[ 7],  8);
    HHH(ddd, eee, aaa, bbb, ccc, X[14],  6);
    HHH(ccc, ddd, eee, aaa, bbb, X[ 6],  6);
    HHH(bbb, ccc, ddd, eee, aaa, X[ 9], 14);
    HHH(aaa, bbb, ccc, ddd, eee, X[11], 12);
    HHH(eee, aaa, bbb, ccc, ddd, X[ 8], 13);
    HHH(ddd, eee, aaa, bbb, ccc, X[12],  5);
    HHH(ccc, ddd, eee, aaa, bbb, X[ 2], 14);
    HHH(bbb, ccc, ddd, eee, aaa, X[10], 13);
    HHH(aaa, bbb, ccc, ddd, eee, X[ 0], 13);
    HHH(eee, aaa, bbb, ccc, ddd, X[ 4],  7);
    HHH(ddd, eee, aaa, bbb, ccc, X[13],  5);
    GGG(ccc, ddd, eee, aaa, bbb, X[ 8], 15);
    GGG(bbb, ccc, ddd, eee, aaa, X[ 6],  5);
    GGG(aaa, bbb, ccc, ddd, eee, X[ 4],  8);
    GGG(eee, aaa, bbb, ccc, ddd, X[ 1], 11);
    GGG(ddd, eee, aaa, bbb, ccc, X[ 3], 14);
    GGG(ccc, ddd, eee, aaa, bbb, X[11], 14);
    GGG(bbb, ccc, ddd, eee, aaa, X[15],  6);
    GGG(aaa, bbb, ccc, ddd, eee, X[ 0], 14);
    GGG(eee, aaa, bbb, ccc, ddd, X[ 5],  6);
    GGG(ddd, eee, aaa, bbb, ccc, X[12],  9);
    GGG(ccc, ddd, eee, aaa, bbb, X[ 2], 12);
    GGG(bbb, ccc, ddd, eee, aaa, X[13],  9);
    GGG(aaa, bbb, ccc, ddd, eee, X[ 9], 12);
    GGG(eee, aaa, bbb, ccc, ddd, X[ 7],  5);
    GGG(ddd, eee, aaa, bbb, ccc, X[10], 15);
    GGG(ccc, ddd, eee, aaa, bbb, X[14],  8);
    FFF(bbb, ccc, ddd, eee, aaa, X[12],  8);
    FFF(aaa, bbb, ccc, ddd, eee, X[15],  5);
    FFF(eee, aaa, bbb, ccc, ddd, X[10], 12);
    FFF(ddd, eee, aaa, bbb, ccc, X[ 4],  9);
    FFF(ccc, ddd, eee, aaa, bbb, X[ 1], 12);
    FFF(bbb, ccc, ddd, eee, aaa, X[ 5],  5);
    FFF(aaa, bbb, ccc, ddd, eee, X[ 8], 14);
    FFF(eee, aaa, bbb, ccc, ddd, X[ 7],  6);
    FFF(ddd, eee, aaa, bbb, ccc, X[ 6],  8);
    FFF(ccc, ddd, eee, aaa, bbb, X[ 2], 13);
    FFF(bbb, ccc, ddd, eee, aaa, X[13],  6);
    FFF(aaa, bbb, ccc, ddd, eee, X[14],  5);
    FFF(eee, aaa, bbb, ccc, ddd, X[ 0], 15);
    FFF(ddd, eee, aaa, bbb, ccc, X[ 3], 13);
    FFF(ccc, ddd, eee, aaa, bbb, X[ 9], 11);
    FFF(bbb, ccc, ddd, eee, aaa, X[11], 11);

    /* combine */
    ddd            += rmd->buffer[1] + cc;
    rmd->buffer[1]  = rmd->buffer[2] + dd + eee;
    rmd->buffer[2]  = rmd->buffer[3] + ee + aaa;
    rmd->buffer[3]  = rmd->buffer[4] + aa + bbb;
    rmd->buffer[4]  = rmd->buffer[0] + bb + ccc;
    rmd->buffer[0]  = ddd;
}

void ripemd_finish(ripemd_t *rmd, const uint8_t *strptr, size_t lswlen, size_t mswlen) {
    size_t   i;
    uint32_t X[16];

    memset(X, 0, 16 * sizeof(uint32_t));

    for (i = 0; i < (lswlen & 63); i++)
        X[i >> 2] ^= (uint32_t)*strptr++ << (8 * (i & 3));

    X[(lswlen >> 2) & 15] ^= (uint32_t)1 << (8 * (lswlen & 3) + 7);

    if ((lswlen & 63) > 55) {
        ripemd_compress(rmd, X);
        memset(X, 0, 16 * sizeof(uint32_t));
    }

    /* append length in bits*/
    X[14] = lswlen << 3;
    X[15] = (lswlen >> 29) | (mswlen << 3);

    ripemd_compress(rmd, X);
}


unsigned char *ripemd_compute(ripemd_t *rmd, const void *data, size_t length) {
    static unsigned char  hashcode[160 / 8];
    const  unsigned char *message = (const unsigned char *)data;

    uint32_t block[16];
    ripemd_reset(rmd);

    for (size_t nbytes = length; nbytes > 63; nbytes -= 64) {
        for (size_t i = 0; i < 16; i++) {
            block[i] = BYTES_TO_DWORD(message);
            message += 4;
        }
        ripemd_compress(rmd, block);
    }

    ripemd_finish(rmd, message, length, 0);

    for (size_t i = 0; i < 160 / 8; i += 4) {
        hashcode[i]     = (uint8_t)(rmd->buffer[i >> 2]);
        hashcode[i + 1] = (uint8_t)(rmd->buffer[i >> 2] >>  8);
        hashcode[i + 2] = (uint8_t)(rmd->buffer[i >> 2] >> 16);
        hashcode[i + 3] = (uint8_t)(rmd->buffer[i >> 2] >> 24);
    }
    return hashcode;
}
