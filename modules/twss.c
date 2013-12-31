#include <module.h>
#include <string.h>

MODULE_ALWAYS(twss);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    regexpr_t *reg = regexpr_create(".*that.*what.*he.*said.*", true);
    if (!reg)
        return;

    if (regexpr_execute(reg, message, 0, NULL))
        irc_write(irc, channel, "%s: They've said it a total of [undefined] times", user);
}
