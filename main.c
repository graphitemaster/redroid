#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "ircman.h"
#include "config.h"
#include "sock.h"
#include "list.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static bool signal_shutdown(bool shutdown) {
    static bool stage = false;
    if (shutdown)
        stage = true;
    return !stage;
}

static void signal_daemonize(bool closehandles) {
    pid_t pid;
    pid_t sid;

    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    umask(0);

    if ((sid = setsid() < 0))
        exit(EXIT_FAILURE);

    if (!closehandles)
        return;

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

static void signal_handle(int signal) {
    if (signal == SIGTERM || signal == SIGINT)
        printf("Recieved shutdown signal\n");
    else if (signal == SIGUSR1)
        printf("Recieved internal error\n");
    else if (signal == SIGHUP)
        return signal_daemonize(true);
    else
        return;

    signal_shutdown(true);
    printf("Shutting down ...\n");
    fflush(NULL);
}

static void signal_install(void) {
    signal(SIGUSR1, &signal_handle);
    signal(SIGTERM, &signal_handle);
    signal(SIGINT,  &signal_handle);
    signal(SIGHUP,  &signal_handle);
}

int main(int argc, char **argv) {
    argc--;
    argv++;

    irc_manager_t *manager = NULL;

    signal_install();
    srand(time(0));

    if (argc && argv[0][0] == '-') {
        switch (argv[0][1]) {
            case 'l': // logging
                freopen(&argv[0][3], "w", stdout);
                freopen(&argv[0][3], "w", stderr);
                signal_daemonize(false);
                break;

            case 'q': // quiet
                freopen("/dev/null", "w", stdout);
                freopen("/dev/null", "w", stderr);
                signal_daemonize(false);
                break;

            case 'd': // daemonize
                signal_daemonize(true);
                break;
        }
    }

    if (!(manager = irc_manager_create())) {
        fprintf(stderr, "failed creating irc manager\n");
        return EXIT_FAILURE;
    }

    list_t *list = config_load("config.ini");
    if (!list) {
        fprintf(stderr, "failed loading configuration\n");
        irc_manager_destroy(manager);
        return EXIT_FAILURE;
    }

    // create all the appropriate IRC instances
    list_iterator_t *it = list_iterator_create(list);
    while (!list_iterator_end(it)) {
        config_t *entry = list_iterator_next(it);
        irc_t    *irc   = irc_create(entry);

        // add all modules
        list_iterator_t *jt = list_iterator_create(entry->modules);
        while (!list_iterator_end(jt))
            irc_modules_add(irc, (const char *)list_iterator_next(jt));
        list_iterator_destroy(jt);

        // and all channels
        jt = list_iterator_create(entry->channels);
        while (!list_iterator_end(jt))
            irc_channels_add(irc, (const char *)list_iterator_next(jt));
        list_iterator_destroy(jt);

        irc_connect(irc, entry->host, entry->port);
        irc_manager_add(manager, irc);

    }

    list_iterator_destroy(it);
    config_unload(list); // unload config

    while (signal_shutdown(false))
        irc_manager_process(manager);

    irc_manager_destroy(manager);
    return EXIT_SUCCESS;
}
