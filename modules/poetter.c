#include <module.h>
#include <stdlib.h>

MODULE_DEFAULT(poetter);

//
// normally I would use a database for these sorts of things
// but since there isn't much shit lennart has said it's easier
// to just maintain a static list here for now.
//
static const char *list[] = {
    "My name is \"Poettering\", with \"oe\", as in \"Goethe\". Not \"Pöttering\".",
    "Do you hate handicapped people? -- Interrupting the speaker's confusion about why GDM needs to run half a GNOME session during a talk.",
    "(Yes, udev on non-systemd systems is in our eyes a dead end, in case you haven't noticed it yet. I am looking forward to the day when we can drop that support entirely.)",
    "I am hacker",
    "I tend to run my builds from within emacs.",
    "Doing this tty dance won't help you much with background tasks such as man-db, updatedb and cron and its jobs, will it? They don't have ttys. Sorry for you. meh! Meh! meh! meh! meh!"
};


void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    size_t entry = rand() % (sizeof(list) / sizeof(*list));
    irc_write(irc, channel, "%s: shit Lennart Poettering says: ``%s''", user, list[entry]);
}
