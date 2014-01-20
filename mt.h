#ifndef REDROID_MT_HDR
#define REDROID_MT_HDR
#include <stdint.h>

typedef struct mt_s mt_t;

mt_t *mt_create(void);
void mt_destroy(mt_t *mt);
uint32_t mt_urand(mt_t *mt);
double mt_drand(mt_t *mt);

#endif
