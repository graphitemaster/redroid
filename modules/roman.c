#include <module.h>
#include <string.h>

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

static const roman_map_t *roman_find(const char ch) {
    for (size_t i = 0; i < sizeof(roman_table)/sizeof(*roman_table); i++)
        if (roman_table[i].key == ch)
            return &roman_table[i];
    return NULL;
}

static bool roman_check(const char *string) {
    for (const char *c = string; *c; c++)
        if (!strchr("MDCLXVI", *c))
            return false;
    return true;
}

static void roman_help(irc_t *irc, const char *channel, const char *user) {
    irc_write(irc, channel, "%s: roman [<romannumeral>]", user);
    irc_write(irc, channel, "%s: used -> <decimal>", user);
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!*message)
        return;
    if (!strcmp(message, "-help"))
        return roman_help(irc, channel, user);
    if (!roman_check(message))
        return;

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
