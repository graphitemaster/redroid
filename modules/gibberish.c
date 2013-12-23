#include <module.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

MODULE_DEFAULT(gibberish);

static const char *consonants = "bcdfghjklmnpqrstvwxz";
static const char *vowels     = "aeiouy";

void gibber(string_t *string, size_t max) {
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
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        message = " ";

    size_t x = isdigit(*message) ? *message : (rand() % 36);
    size_t y = 0;

    string_t *string = NULL;
    while (x > y) {
        if (string)
            string_catf(string, " ");
        else
            string = string_construct();
        gibber(string, (isdigit(message[y]) ? message[y] : 16));
        y++;
    }

    irc_write(irc, channel, "%s: %s", user, string_contents(string));
    string_destroy(string);
}
