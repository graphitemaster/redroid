#include <module.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

MODULE_DEFAULT(dur);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    char buffer[16];
    char output[80];
    memset(output, 0, sizeof(output));

    long read = strtol(message, NULL, 10);

    int years   = 0;
    int months  = 0;
    int weeks   = 0;
    int days    = 0;
    int hours   = 0;
    int minutes = 0;
    int seconds = 0;

#   define TERM(TOP, ITEM, FMT)                              \
    do {                                                     \
        if (read >= TOP) {                                   \
            ITEM = read / TOP;                               \
            snprintf(buffer, sizeof(buffer), "%d"FMT, ITEM); \
            strcat(output, buffer);                          \
            read -= (ITEM * TOP);                            \
        }                                                    \
    } while (0)

    TERM(604800,    weeks,   "w");
    TERM(86400,     days,    "d");
    TERM(3600,      hours,   "h");
    TERM(60,        minutes, "m");
    TERM(1,         seconds, "s");

    if (strlen(output))
        irc_write(irc, channel, "%s: %s", user, output);
    else
        irc_write(irc, channel, "%s: 0", user);

#   undef TERM
}
