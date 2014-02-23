#include <module.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

MODULE_DEFAULT(penish);

static const signed char quinbot_table[] = {
    12, 8, 20, 15, 8, 1, 6, 9, 15, -2, -21,
    -2, 5, 12, 19, 5, 8, -12, 6, -2, 14, 9,
    -8, 1, 6, -9, 15, 2, 21, 2, -8, 12, -19,
    5, 8, -20
};

static const size_t botlevel = 0;

void module_enter(irc_t *irc, const char *channel, const char *who, const char *string) {
    if (!string)
        string = who;

    string_t *str    = string_format("%s: ((", who);
    size_t    index  = 0;
    size_t    length = strlen(string);
    int       value  = 0;

    while (index < length) {
        char c = tolower(string[index]);
        char d = c - 96;

        if (d >= 27)
            d = 0 - d;

        d = ((d + quinbot_table[index]) % 30) + 1;
        value += d;

        string_catf(str, "%c[%d]", toupper(c), d);
        index++;
    }

    int bonus = 10 - length + botlevel;
    string_catf(str, ")/LEN[%zu])", length);
    if (bonus >= 0)
        string_catf(str, "+BONUS[%d]", bonus);
    else
        string_catf(str, "-PENALTY[%d]", 0 - bonus);

    irc_write(irc, channel, string_contents(str));

    float cm = (((float)value / length) + bonus);
    float in = (cm * 0.393700787);

    if (bonus >= 0)
        irc_write(irc, channel, "%s: (%d/%zu)+%d = %.2fcm [%.2fin]", who, value, length, bonus, cm, in);
    else
        irc_write(irc, channel, "%s: (%d/%zu)%d = %.2fcm [%.2fin]", who, value, length, bonus, cm, in);
}
