#ifndef REDROID_ACCESS_HDR
#define REDROID_ACCESS_HDR
#include <stdbool.h>

/* The value required to perform access control */
#define ACCESS_CONTROL   4
#define ACCESS_SHITLIST -2
#define ACCESS_IGNORE   -1

#define ACCESS_MIN      -2
#define ACCESS_MAX       6

typedef enum {
    ACCESS_NOEXIST_TARGET,
    ACCESS_NOEXIST_INVOKE,
    ACCESS_EXISTS,
    ACCESS_DENIED,
    ACCESS_SUCCESS,
    ACCESS_FAILED,
    ACCESS_BADRANGE
} access_t;

bool     access_range (irc_t *irc, const char *channel, const char *target, int check);
bool     access_check (irc_t *irc, const char *channel, const char *target, int check);
bool     access_level (irc_t *irc, const char *channel, const char *target, int *level);
access_t access_remove(irc_t *irc, const char *channel, const char *target, const char *invoke);
access_t access_insert(irc_t *irc, const char *channel, const char *target, const char *invoke, int level);
access_t access_change(irc_t *irc, const char *channel, const char *target, const char *invoke, int level);

#define access_ignore(IRC, CHANNEL, TARGET) \
    access_check((IRC), (CHANNEL), (TARGET), ACCESS_IGNORE)

#define access_shitlist(IRC, CHANNEL, TARGET) \
    access_check((IRC), (CHANNEL), (TARGET), ACCESS_SHITLIST)

#endif
