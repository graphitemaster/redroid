#include <module.h>

MODULE_ALWAYS(twss);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        return;

    regexpr_t *reg = regexpr_create(".*that.*what.*he.*said.*", true);
    if (!reg)
        return;

    if (regexpr_execute(reg, message, 0, NULL)) {
        database_request(irc, "TWSS");
        irc_write(irc, channel, "%s: They've said it a total of %d times",
            user,
            database_request_count(irc, "TWSS"));
    }
}
