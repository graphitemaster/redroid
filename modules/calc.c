#include <module.h>
#include <stdlib.h>
#include <stdio.h>

MODULE_DEFAULT(calc);

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!message)
        return;

    // we cheat here and use perl here
    char *command = NULL;
    asprintf(&command, "perl -e 'use Math::Trig; print (%s)'", message);
    FILE *fp = popen(command, "r");
    double value;
    if (fscanf(fp, "%lf", &value) != 1)
        value = 0;
    fclose(fp);
    free(command);

    irc_write(irc, channel, "%s: %g", user, value);
}

void module_close(irc_t *irc) {
    // nothing
}
