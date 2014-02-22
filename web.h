#ifndef REDROID_WEB_HDR
#define REDROID_WEB_HDR

typedef struct web_s web_t;

web_t *web_create(void);
void web_destroy(web_t *web);
void web_begin(web_t *web);
#endif
