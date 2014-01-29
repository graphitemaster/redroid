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
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>

#define SIGNAL_ERROR     (SIGRTMIN + 4)
#define SIGNAL_RESTART   (SIGRTMIN + 5)
#define SIGNAL_DAEMONIZE (SIGRTMIN + 6)

typedef struct {
    char *instance;
    char *channel;
    char *user;
} restart_info_t;

static restart_info_t *restart_info_create(const char *instance, const char *channel, const char *user) {
    restart_info_t *info = malloc(sizeof(*info));

    info->instance = strdup(instance);
    info->channel  = strdup(channel);
    info->user     = strdup(user);

    return info;
}

static void restart_info_destroy(restart_info_t *info) {
    free(info->instance);
    free(info->channel);
    free(info->user);
    free(info);
}

static restart_info_t *restart_info_singleton(restart_info_t *info) {
    static restart_info_t *rest = NULL;
    if (!rest)
        rest = info;
    return rest;
}

void restart(irc_t *irc, const char *channel, const char *user) {
    /* Install information and restart */
    restart_info_singleton(restart_info_create(irc->name, channel, user));
    raise(SIGNAL_RESTART);
}

static const char *build_date() {
    return __DATE__;
}

static const char *build_time() {
    return __TIME__;
}

static bool signal_restart(bool restart) {
    static bool stage = false;
    if (restart)
        stage = true;
    return !stage;
}

static bool signal_shutdown(bool shutdown) {
    static bool stage = false;
    if (shutdown)
        stage = true;
    return !stage;
}

static bool signal_empty(void) {
    return signal_restart(false) && signal_shutdown(false);
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
    bool (*handler)(bool) = NULL;
    const char *message = NULL;

    if (signal == SIGTERM || signal == SIGINT) {
        printf("Recieved shutdown\n");
        handler = &signal_shutdown;
        message = "Shutting down";
    } else if (signal == SIGNAL_ERROR) {
        printf("Recieved internal error\n");
        handler = &signal_shutdown;
        message = "Shutting down";
    } else if (signal == SIGNAL_RESTART) {
        printf("Recieved restart\n");
        handler = &signal_restart;
        message = "Restarting";
    } else if (signal == SIGNAL_DAEMONIZE) {
        return signal_daemonize(true);
    } else {
        raise(SIGNAL_ERROR);
        return;
    }

    if (!handler) {
        raise(SIGNAL_ERROR);
        return;
    }

    handler(true);
    printf("%s ...\n", message);
    fflush(NULL);
}

static void signal_install(void) {
    signal(SIGTERM,          &signal_handle);
    signal(SIGINT,           &signal_handle);
    signal(SIGNAL_ERROR,     &signal_handle);
    signal(SIGNAL_RESTART,   &signal_handle);
    signal(SIGNAL_DAEMONIZE, &signal_handle);
}

static bool signal_restarted(int *argc, char ***argv, int *tmpfd) {
    if (*argc != 2 || strncmp((*argv)[1], "-r", 2))
        return false;
    if (sscanf(&((*argv)[1][2]), "%d", tmpfd) != 1)
        return false;

    /* Check if valid file descriptor */
    struct stat b;
    if (fstat(*tmpfd, &b) == -1)
        return false;

    return true;
}

int main(int argc, char **argv) {
    irc_manager_t *manager = NULL;
    signal_install();
    srand(time(0));

    if (!(manager = irc_manager_create())) {
        fprintf(stderr, "failed creating irc manager\n");
        return EXIT_FAILURE;
    }

    int tmpfd = -1;
    if (signal_restarted(&argc, &argv, &tmpfd)) {
        if (lseek(tmpfd, 0, SEEK_SET) != 0) {
            fprintf(stderr, "%s: restart failed (lseek failed) [%s]\n", *argv, strerror(errno));
            irc_manager_destroy(manager);
            return EXIT_FAILURE;
        }

        /*
         * Read the info line containing all the information about who
         * restarted this instance.
         */
        size_t infosize = 0;
        read(tmpfd, &infosize, sizeof(size_t));
        char *infoline = malloc(infosize);
        read(tmpfd, infoline, infosize);

        size_t networks = 0;
        read(tmpfd, &networks, sizeof(size_t));

        printf("Restart state contains %zu network(s)\n", networks);

        /*
         * Grab the string table from the file and create a list from it;
         * all entries are seperated by newlines.
         */
        size_t offset =
            sizeof(size_t) +         /* infoline strlen       */
            infosize       +         /* infoline length       */
            sizeof(size_t) +         /* file descriptor count */
            sizeof(int) * networks;  /* file descriptors      */

        lseek(tmpfd, offset, SEEK_SET);

        list_t   *list   = list_create();
        string_t *string = string_construct();

        for (;;) {
            char ch;
            /* Read to EOF */
            int r;
            if ((r = read(tmpfd, &ch, 1)) == -1 || r == 0)
                break;

            if (ch == '\n') {
                list_push(list, string);
                string = string_construct();
                continue;
            }

            string_catf(string, "%c", ch);
        }

        /* Load configuration */
        list_t *config = config_load("config.ini");

        /* Now back to where sockets themselfs are stored */
        lseek(tmpfd, sizeof(size_t) * 2 + infosize, SEEK_SET);

        list_iterator_t *it   = list_iterator_create(list);
        string_t        *name = NULL;
        int              sock = 0;

        for (size_t i = 0; i < networks; i++) {
            name = list_iterator_next(it);
            read(tmpfd, &sock, sizeof(int));
            printf("    Network %s on socket %d will be restored\n", string_contents(name), sock);

            /* Find configuration for that instance */
            config_t        *entry = NULL;
            list_iterator_t *ci    = list_iterator_create(config);
            while (!list_iterator_end(ci)) {
                entry = list_iterator_next(ci);
                if (!strcmp(string_contents(name), entry->name))
                    break;
            }

            /*
             * Create instance and add all appropriate modules and or
             * channels then set the ready status since we're already
             * connected. This should be sufficent enough to pickup
             * where we originally left off.
             */
            irc_t *instance = irc_create(entry);
            list_iterator_t *jt = list_iterator_create(entry->modules);
            while (!list_iterator_end(jt))
                irc_modules_add(instance, (const char *)list_iterator_next(jt));
            list_iterator_destroy(jt);

            jt = list_iterator_create(entry->channels);
            while (!list_iterator_end(jt))
                irc_channels_add(instance, (const char *)list_iterator_next(jt));
            list_iterator_destroy(jt);

            /*
             * Readying status make it such that it assumes it's connected
             * then add it to the manager to be processed. We also reinstate
             * the connection.
             */
            if (!irc_reinstate(instance, entry->host, entry->port, entry->ssl, sock)) {
                /* Something went terribly wrong in the reinstate process
                 * TODO: handle it some how
                 */
                abort();
            }

            instance->ready    = true;
            instance->readying = true;
            irc_manager_add(manager, instance);

            list_iterator_destroy(ci);
            string_destroy(name);
        }

        /*
         * The infoline can now be parsed and we can find the appropriate
         * instance and stuff the command down the stream for that instance
         * so we can print what happened.
         */
        char *instance = strdup(strtok(infoline, "|"));
        char *channel  = strdup(strtok(NULL,     "|"));
        char *user     = strdup(strtok(NULL,     "|"));
        char *date     = strdup(strtok(NULL,     "|"));
        char *time     = strdup(strtok(NULL,     "|"));

        irc_t *update = irc_manager_find(manager, instance);
        irc_write(update, channel, "%s: succesfully restarted\n",
            user, date, time, build_date(), build_time());

        if (strcmp(date, build_date()) || strcmp(time, build_time())) {
            irc_write(update, channel, "%s: last instance build timestamp %s %s", user, date, time);
            irc_write(update, channel, "%s: this instance build timestamp %s %s", user, build_date(), build_time());
        } else {
            irc_write(update, channel, "%s: this instance is the same binary as last instance", user);
        }

        free(time);
        free(date);
        free(user);
        free(channel);
        free(instance);

        config_unload(config);
        list_iterator_destroy(it);
        list_destroy(list);

    } else {
        /*
         * Non restart state parse command line options and deal with a
         * all the other non interesting things like typical instance
         * creation and what not.
         */
        if (argc == 2 && argv[1][0] == '-') {
            switch (argv[1][1]) {
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

        list_t *list = config_load("config.ini");
        if (!list) {
            fprintf(stderr, "failed loading configuration (see config.ini.example)\n");
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

            if (!irc_connect(irc, entry->host, entry->port, entry->ssl)) {
                irc_destroy(irc, false);
                fprintf(stderr, "    irc      => cannot connect (ignoring instance)\n");
                continue;
            }
            irc_manager_add(manager, irc);
        }

        list_iterator_destroy(it);
        config_unload(list); // unload config
    }

    if (irc_manager_empty(manager)) {
        fprintf(stderr, "No IRC instances to manage\n");
        irc_manager_destroy(manager);
        return EXIT_FAILURE;
    }

    while (signal_empty())
        irc_manager_process(manager);

    if (!signal_restart(false)) {
        list_t *fds = irc_manager_restart(manager); printf("G\n");

        char unique[] = "redroid_XXXXXX";
        int fd = mkstemp(unique);
        if (fd == -1) {
            fprintf(stderr, "%s: restart failed (mkstemp failed) [%s]\n", *argv, strerror(errno));
            irc_manager_destroy(manager);
            return EXIT_FAILURE;
        }

        /*
         * Get the singleton of information containing who and from
         * where the restart originated from. Including this current
         * processes build date and time stamp.
         */
        restart_info_t *restinfo = restart_info_singleton(NULL);
        string_t *infoline = string_format("%s|%s|%s|%s|%s",
            restinfo->instance,
            restinfo->channel,
            restinfo->user,
            build_date(),
            build_time());

        /*
         * Calculate the length of the info line and store it as the
         * very first thing to the restart file. Then write the info
         * line to the file.
         */
        size_t infosize = string_length(infoline) + 1;  printf("A\n");
        write(fd, &infosize, sizeof(size_t));           printf("B\n");
        write(fd, string_contents(infoline), infosize); printf("C\n");
        string_destroy(infoline);                       printf("D\n");
        restart_info_destroy(restinfo);                 printf("E\n");

        /* String table will be stored at the end of the file */
        string_t *stringtable = string_construct();     printf("F\n");

        /* Store number of file descriptors */
        size_t count = list_length(fds);                printf("H\n");

        write(fd, &count, sizeof(size_t));              printf("I\n");
        printf("Restart state with %zu network(s):\n", count);

        /* Now deal with the file descriptors */
        list_iterator_t *it = list_iterator_create(fds);
        while (!list_iterator_end(it)) {
            irc_manager_restart_t *e = list_iterator_next(it);
            printf("    Network %s on socket %d stored\n", e->name, e->fd);
            write(fd, &e->fd, sizeof(int));

            /* Apeend to string table seperated by newlines */
            string_catf(stringtable, "%s\n", e->name);

            free(e->name);
            free(e);
        }

        /* Write string table */
        write(fd, string_contents(stringtable), string_length(stringtable));
        string_destroy(stringtable);

        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "-r%d", fd);
        execv(*argv, (char *[]){ *argv, buffer, (char *)NULL });
    }

    irc_manager_destroy(manager);
    return EXIT_SUCCESS;
}
