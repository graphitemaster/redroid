#include <module.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

MODULE_DEFAULT(gibberish);

static const char *consonants = "bcdfghjklmnpqrstvwxz";
static const char *vowels     = "aeiouy";

bool gibber(string_t *string, size_t max) {
    if (!max)
        max = 1;

    size_t a = rand() % max;
    size_t b = 1;
    size_t c = 0;
    size_t v = 0;

    while (a >= b) {
        size_t q = (c >= 2) ? 0 : (v >= 2) ? 1 : (rand() % 1);

        if (q != 1) {
            string_catf(string, "%c", vowels[rand() % strlen(vowels)]);
            v++;
            c = 0;
        } else {
            string_catf(string, "%c", consonants[rand() % strlen(consonants)]);
            c++;
            v = 0;
        }
        b++;
    }

    return b > 1;
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        message = user;

    const char *digit = message;
    while (*digit && isspace(*digit))
        digit++;

    size_t x = isdigit(*digit) ? atoi(message) : (rand() % 36);
    size_t y = 0;

    while (*digit && isdigit(*digit)) digit++;
    while (*digit && isspace(*digit)) digit++;

    string_t *string = string_construct();
    while (x > y) {
        if (gibber(string, (isdigit(*digit) ? atoi(digit) : 16)) && y+1 != x)
            string_catf(string, " ");
        y++;
    }

    irc_write(irc, channel, "%s: %s", user, string_contents(string));
    string_destroy(string);
}
