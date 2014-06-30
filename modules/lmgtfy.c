#include <module.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

MODULE_DEFAULT(lmgtfy);

static char *lmgtfy_encode(const char *input) {
    const char *str = input;
    char       *buf = malloc(strlen(input) * 3 + 1);
    char       *ptr = buf;
    while (*str) {
        if (isalnum(*str) || strchr("-_.~", *str))
            *ptr++ = *str;
        else if (*str == ' ')
            *ptr++ = '+';
        else {
            *ptr++ = '%';
            *ptr++ = "0123456789abcdef"[(*str >> 4) & 15];
            *ptr++ = "0123456789abcdef"[*str & 15];
        }
        str++;
    }
    *ptr = '\0';
    return buf;
}

static void lmgtfy_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: lmgtfy <target> <query>", user);
    irc_write(irc, channel, "%s: used <target>: <lmgtfy url>", user);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    list_t *split  = strnsplit(message, " ", 1);
    char   *method = list_shift(split);
    char   *what   = list_shift(split);

    if (!method || !strcmp(method, "-help"))
        return lmgtfy_help(irc, channel, user);

    irc_write(irc, channel, "%s: http://lmgtfy.com/?q=%s", method, lmgtfy_encode(what));
}
