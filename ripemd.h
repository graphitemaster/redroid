#ifndef REDROID_RIPEMD_HDR
#define REDROID_RIPEMD_HDR
#include <stdint.h>

typedef struct ripemd_s ripemd_t;

ripemd_t *ripemd_create(void);
void ripemd_destroy(ripemd_t *rmd);

void ripemd_finish(ripemd_t *rmd, const uint8_t *strptr, size_t lswlen, size_t mswlen);
void ripemd_compress(ripemd_t *rmd, uint32_t *block);
unsigned char *ripemd_compute(ripemd_t *rmd, const void *data, size_t length);

#endif
