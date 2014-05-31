#include <module.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

MODULE_DEFAULT(roman);

typedef struct{
    const char   key;
    const size_t val;
} roman_map_t;

static roman_map_t roman_table[] = {
    { 'M', 1000 }, { 'D', 500  },
    { 'C', 100  }, { 'L', 50   },
    { 'X', 10   }, { 'V', 5    },
    { 'I', 1    }
};

static const char *roman_mapping[4][10] = {
    {"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"},
    {"", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC"},
    {"", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM"},
    {"", "M", "MM", "MMM"}
};

static const roman_map_t *roman_find(const char ch) {
    for (size_t i = 0; i < sizeof(roman_table)/sizeof(*roman_table); i++)
        if (roman_table[i].key == ch)
            return &roman_table[i];
    return NULL;
}

static bool roman_check_roman(const char *string) {
    for (const char *c = string; *c; c++)
        if (!strchr("MDCLXVI", *c))
            return false;
    return true;
}

static bool roman_check_decimal(const char *string) {
    for (const char *c = string; *c; c++)
        if (!strchr("0123456789", *c))
            return false;
    return true;
}

static void roman_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: roman [<romannumeral> | <decimal>]", user);
    irc_write(irc, channel, "%s: used <romannumeral> -> <decimal>", user);
    irc_write(irc, channel, "%s: used <decimal> -> <romannumeral>", user);
}

static void roman_roman(irc_t *irc, const char *channel, const char *user, const char *message) {
    int    r = 0;
    size_t l = strlen(message);
    for (size_t i = 0; i < l - 1; ++i) {
        const roman_map_t *a = roman_find(message[i]);
        const roman_map_t *b = roman_find(message[i + 1]);
        if (a->val < b->val)
            r -= a->val;
        else
            r += a->val;
    }
    r += roman_find(message[l - 1])->val;

    irc_write(irc, channel, "%s: %d", user, r);
}

static void roman_decimal(irc_t *irc, const char *channel, const char *user, const char *message) {
    string_t *r = string_construct();
    int       n = atoi(message);
    int       i = 3;

    if (n > 3999 || n < 1)
        return irc_write(irc, channel, "%s: must be between 1 and 4999", user);

    while (n > 0) {
        int d = (int)pow(10, i);
        const char *get;
        if ((get = roman_mapping[i][n / d]))
            string_catf(r, "%s", get);
        n %= d;
        i--;
    }

    irc_write(irc, channel, "%s: %s", user, string_contents(r));
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!*message)
        return roman_help(irc, channel, user);
    if (!strcmp(message, "-help"))
        return roman_help(irc, channel, user);
    if (!roman_check_roman(message)) {
        if (roman_check_decimal(message))
            return roman_decimal(irc, channel, user, message);
        return;
    }
    return roman_roman(irc, channel, user, message);
}
