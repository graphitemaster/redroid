#include <stdio.h>  // printf, fprintf
#include <stdlib.h> // EXIT_SUCCESS, EXIT_FAILURE
#include <signal.h> // signal, SIGINT
#include <string.h>

#include "ircman.h"
#include "config.h"
#include "sock.h"
#include "list.h"

// signal safe singleton, can consistently recieve a signal and
// overwrite the stage without working about odd race conditions
// i.e multiple CTRL+C to force quit.
static bool signal_shutdown(bool shutdown) {
    static bool stage = false;
    if (shutdown)
        stage = true;
    return !stage;
}

static void signal_handle(int signal) {
    signal_shutdown(true);

    if (signal == SIGTERM || signal == SIGINT)
        printf("Recieved shutdown signal\n");
    else
        printf("Recieved internal error\n");

    printf("Shutting down ...\n");
    fflush(NULL);
}

static void signal_install(void) {
    signal(SIGUSR1, &signal_handle);
    signal(SIGTERM, &signal_handle);
    signal(SIGINT,  &signal_handle);
}

int main() {
    irc_manager_t *manager;

    signal_install();

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
        irc_t    *irc   = irc_create(entry->name, entry->nick);

        irc_connect(irc, entry->host, entry->port);
        irc_manager_add(manager, irc);
    }
    list_iterator_destroy(it);
    config_unload(list); // unload it

    while (signal_shutdown(false))
        irc_manager_process(manager);

    irc_manager_destroy(manager);
    return EXIT_SUCCESS;
}
