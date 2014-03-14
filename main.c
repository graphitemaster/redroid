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
#include "web.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>

#define SIGNAL_ERROR     (SIGRTMIN + 4)

#define RESTART_FILENAME  "redroid_XXXXXX"
#define RESTART_FILESIZE  sizeof(RESTART_FILENAME)
#define RESTART_MAGICDATA "Redroid"
#define RESTART_MAGICSIZE sizeof(RESTART_MAGICDATA)

#define REDROID_LOCKFILE  "Redroid.pid"

extern const char *build_date();
extern const char *build_time();

static char *redroid_binary;

static bool signal_restart(bool restart);
static bool signal_shutdown(bool shutdown);

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

void redroid_restart_global(irc_manager_t *manager) {
    /* global is only called via the webclient */
    restart_info_singleton(restart_info_create(";webclient", ";webclient", ";webclient"));
    signal_restart(true);
    irc_manager_wake(manager);
}

void redroid_shutdown_global(irc_manager_t *manager) {
    signal_shutdown(true);
    irc_manager_wake(manager);
}

void redroid_restart(irc_t *irc, const char *channel, const char *user) {
    /* Install information and restart */
    restart_info_singleton(restart_info_create(irc->name, channel, user));
    signal_restart(true);
    irc_manager_wake(irc->manager);
}

void redroid_shutdown(irc_t *irc, const char *channel, const char *user) {
    /* todo print message */
    redroid_shutdown_global(irc->manager);
}

void redroid_abort(void) {
    /* We need atexit handlers to be called for the lock file */
    fprintf(stderr, "Aborted\n");
    exit(EXIT_FAILURE);
}

void redroid_recompile(irc_t *irc, const char *channel, const char *user) {
    string_t *backupname = NULL;
    const char *error;

    /* Backup the instance binary */
    FILE *fp;
    if (!(fp = fopen(redroid_binary, "rb"))) {
        error = "backing up redroid binary (failed opening for backup) ";
        goto redroid_recompile_fail;
    }

    fseek(fp, 0, SEEK_END);
    size_t binsize = ftell(fp);
    char  *binread = malloc(binsize);
    fseek(fp, 0, SEEK_SET);

    if (fread(binread, binsize, 1, fp) != 1) {
        fclose(fp);
        error = "backing up redroid binary (failed reading for backup)";
        goto redroid_recompile_fail;
    }

    fclose(fp);

    backupname = string_format("%s.bak", redroid_binary);
    if (!(fp = fopen(string_contents(backupname), "wb"))) {
        error = "backing up redroid binary (failed creating backup file)";
        goto redroid_recompile_fail;
    }

    if (fwrite(binread, binsize, 1, fp) != 1) {
        error = "backing up redroid binary (failed writing backup file)";
        goto redroid_recompile_fail;
    }

    /* Make sure the binary can be executed */
    if (chmod(string_contents(backupname), S_IRWXU) != 0) {
        error = "backing up redroid binary (failed setting executable permissions for backup file)";
        goto redroid_recompile_fail;
    }

    fclose(fp);

    /* Cleanup the instance. This will erase the binary */
    pclose(popen("make clean", "r"));

    /* Now try the recompile */
    if (!(fp = popen("make 2>&1", "r")))
        irc_write(irc, channel, "%s: failed to recompile", user);

    list_t *lines = list_create();
    char   *line  = NULL;
    size_t  size  = 0;

    while (getline(&line, &size, fp) != EOF)
        list_push(lines, strdup(line));
    free(line);

    if (pclose(fp) != 0) {
        /* it failed to recompile, search for errors */
        list_t *errors = list_create();
        while ((line = list_shift(lines))) {
            if (strstr(line, "error:"))
                list_push(errors, line);
            else
                free(line);
        }

        size_t count = list_length(errors);
        irc_write(irc, channel, "%s: recompile failed (%zu %s)",
            user, count, (count == 1) ? "error" : "errors");

        if (count > 5)
            irc_write(irc, channel, "%s: showing only the first five errors", user);

        for (size_t i = 0; i < 5 && (line = list_shift(errors)); i++) {
            irc_write(irc, channel, "%s: %s", user, line);
            free(line);
        }
        list_foreach(errors, &free);
        list_destroy(errors);
        rename(string_contents(backupname), redroid_binary);
    } else {
        /* if everything went fine then do the restart */
        irc_write(irc, channel, "%s: recompiled successfully", user);
        redroid_restart(irc, channel, user);
        remove(string_contents(backupname));
    }

    string_destroy(backupname);
    list_foreach(lines, &free);
    list_destroy(lines);
    return;

redroid_recompile_fail:
    /* backupname can be allocated from a path coming to here */
    if (backupname)
        string_destroy(backupname);
    /* fp can be opened from a path coming to here */
    if (fp)
        fclose(fp);

    irc_write(irc, channel, "%s: failed recompiling", user);
    irc_write(irc, channel, "%s: %s", user, error);
}

static void redroid_unlock(void) {
    unlink(REDROID_LOCKFILE);
}

static void redroid_lock(void) {
    FILE *fp = fopen(REDROID_LOCKFILE, "w");
    if (fp) {
        atexit(redroid_unlock);
        fclose(fp);
    }
}

static bool redroid_locked(void) {
    FILE *fp;
    if (!(fp = fopen(REDROID_LOCKFILE, "r"))) {
        redroid_lock();
        return false;
    }
    fclose(fp);
    return true;
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
    if (!signal_restart(false))  return false;
    if (!signal_shutdown(false)) return false;
    return true;
}

static void signal_daemonize(bool closehandles) {
    pid_t pid;
    pid_t sid;

    pid = fork();
    if (pid == -1)
        exit(EXIT_FAILURE);
    if (pid != 0)
        exit(EXIT_SUCCESS);

    if ((sid = setsid()) == -1)
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
    } else
        goto signal_error;
    if (!handler)
        goto signal_error;

    handler(true);
    printf("%s ...\n", message);
    fflush(NULL);
    return;

signal_error:
    raise(SIGNAL_ERROR);
}

static void signal_install(void) {
    signal(SIGTERM,          &signal_handle);
    signal(SIGINT,           &signal_handle);
    signal(SIGNAL_ERROR,     &signal_handle);
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

    /*
     * Check the contents of the file to validate that it is indeed a
     * restart file. This is done by checking the first bytes for
     * RESTART_MAGICDATA
     */
    lseek(*tmpfd, 0, SEEK_SET);
    char magic[RESTART_MAGICSIZE];
    read(*tmpfd, magic, sizeof(magic));
    if (strcmp(magic, RESTART_MAGICDATA)) {
        close(*tmpfd);
        return false;
    }

    return true;
}

int main(int argc, char **argv) {
    irc_manager_t *manager = NULL;
    web_t         *web     = NULL;

    if (redroid_locked()) {
        fprintf(stderr, "an instance of Redroid is already running\n");
        return EXIT_FAILURE;
    }

    /*
     * We need the binary name to perform backups on it for recompiling
     * the bot within itself.
     */
    redroid_binary = *argv;

    signal_install();
    srand(time(0));

    if (!(manager = irc_manager_create())) {
        fprintf(stderr, "failed creating irc manager\n");
        return EXIT_FAILURE;
    }

    if (!(web = web_create())) {
        fprintf(stderr, "failed creating webfront server\n");
        irc_manager_destroy(manager);
        return EXIT_FAILURE;
    }

#if 0
    web_begin(web, manager);
    while (signal_empty())
        ;
    web_destroy(web);
    irc_manager_destroy(manager);
    return EXIT_SUCCESS;
#endif

    int tmpfd = -1;
    if (signal_restarted(&argc, &argv, &tmpfd)) {
        if (lseek(tmpfd, 8, SEEK_SET) != 8) { /* 8 bytes to skip 'Redroid\0' */
            fprintf(stderr, "%s: restart failed (lseek failed) [%s]\n", *argv, strerror(errno));
            irc_manager_destroy(manager);
            return EXIT_FAILURE;
        }

        char tmpfilename[RESTART_FILESIZE];
        read(tmpfd, tmpfilename, sizeof(tmpfilename));

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

        /* Calculate the string table offset */
        size_t strtabof = 0;

        strtabof += 8;                      /* 'Redroid\0'       */
        strtabof += sizeof(tmpfilename);    /* this filename     */
        strtabof += sizeof(size_t);         /* infoline size     */
        strtabof += infosize;               /* infoline itself   */
        strtabof += sizeof(size_t);         /* number of sockets */
        strtabof += sizeof(int) * networks; /* all the sockets   */

        lseek(tmpfd, strtabof, SEEK_SET);

        list_t   *list   = list_create();
        string_t *string = string_construct();

        for (;;) {
            char ch;
            /* Read to EOF */
            int r;
            if ((r = read(tmpfd, &ch, 1)) == -1 || r == 0) {
                string_destroy(string);
                break;
            }

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
        lseek(tmpfd, 8 + RESTART_FILESIZE + (sizeof(size_t) * 2) + infosize, SEEK_SET);

        string_t *name = NULL;
        int       sock = 0;

        for (size_t i = 0; i < networks; i++) {
            read(tmpfd, &sock, sizeof(int)); /* Get socket */

            name = list_at(list, i);
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

            /* Prepare the restart data */
            sock_restart_t restdata = {
                .ssl  = entry->ssl,
                .fd   = sock,
            };

            if (!irc_reinstate(instance, entry->host, entry->port, &restdata))
                redroid_abort();

            instance->flags |= IRC_STATE_READY;
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

        /* no suitable restart information whenc oming from web client */
        if (strcmp(instance, ";webclient")) {
            irc_t *update = irc_manager_find(manager, instance);
            irc_write(update, channel, "%s: successfully restarted", user);

            if (strcmp(date, build_date()) || strcmp(time, build_time())) {
                irc_write(update, channel, "%s: last instance build timestamp %s %s", user, date, time);
                irc_write(update, channel, "%s: this instance build timestamp %s %s", user, build_date(), build_time());
            } else {
                irc_write(update, channel, "%s: this instance is the same binary as last instance", user);
            }
        } else {
            /* broadcast to all channels and servers */
            irc_manager_broadcast(manager, "successfully restarted");
            if (strcmp(date, build_date()) || strcmp(time, build_time())) {
                irc_manager_broadcast(manager, "last instance build timestamp %s %s", date, time);
                irc_manager_broadcast(manager, "this instance build timestamp %s %s", build_date(), build_time());
            } else {
                irc_manager_broadcast(manager, "this instance is the same binary as last instance");
            }
        }

        unlink(tmpfilename);

        free(time);
        free(date);
        free(user);
        free(channel);
        free(instance);
        free(infoline);

        config_unload(config);
        list_destroy(list);

    } else {
        /*
         * Non restart state parse command line options and deal with a
         * all the other non interesting things like typical instance
         * creation and what not.
         */
        if (argc == 2 && argv[1][0] == '-') {
            switch (argv[1][1]) {
                case 'l': /* Logging */
                    freopen(&argv[0][3], "w", stdout);
                    freopen(&argv[0][3], "w", stderr);
                    signal_daemonize(false);
                    break;

                case 'q': /* Quiet */
                    freopen("/dev/null", "w", stdout);
                    freopen("/dev/null", "w", stderr);
                    signal_daemonize(false);
                    break;

                case 'd': /* Daemonize */
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

        /* Create all IRC instances */
        list_iterator_t *it = list_iterator_create(list);
        while (!list_iterator_end(it)) {
            config_t *entry = list_iterator_next(it);
            irc_t    *irc   = irc_create(entry);

            /* Add all the modules for this instance */
            list_iterator_t *jt = list_iterator_create(entry->modules);
            while (!list_iterator_end(jt))
                irc_modules_add(irc, (const char *)list_iterator_next(jt));
            list_iterator_destroy(jt);

            /* Add all the channels for this instance */
            jt = list_iterator_create(entry->channels);
            while (!list_iterator_end(jt))
                irc_channels_add(irc, (const char *)list_iterator_next(jt));
            list_iterator_destroy(jt);

            if (!irc_connect(irc, entry->host, entry->port, entry->ssl)) {
                irc_destroy(irc, NULL, NULL);
                fprintf(stderr, "    irc      => cannot connect (ignoring instance)\n");
                continue;
            }
            irc_manager_add(manager, irc);
        }

        list_iterator_destroy(it);
        config_unload(list);
    }

    if (irc_manager_empty(manager)) {
        fprintf(stderr, "No IRC instances to manage\n");
        irc_manager_destroy(manager);
        web_destroy(web);
        return EXIT_FAILURE;
    }

    /* Start the web frontend */
    web_begin(web, manager);

    while (signal_empty()) {
        irc_manager_process(manager);
    }

    web_destroy(web);

    if (!signal_restart(false)) {
        list_t *fds = irc_manager_restart(manager);
        char unique[RESTART_FILESIZE] = RESTART_FILENAME;
        int fd = mkstemp(unique);
        if (fd == -1) {
            fprintf(stderr, "%s: restart failed (mkstemp failed) [%s]\n", *argv, strerror(errno));
            return EXIT_FAILURE;
        }

        /*
         * Get the singleton of information containing who and from
         * where the restart originated from. Including this current
         * processes build date and time stamp.
         */
        restart_info_t *restinfo = restart_info_singleton(NULL);
        if (!restinfo) {
            fprintf(stderr, "%s: restart failed (no restart info)\n", *argv);
            unlink(unique);
            return EXIT_FAILURE;
        }

        /* Write the signature */
        write(fd, RESTART_MAGICDATA, RESTART_MAGICSIZE);

        /* Write the filename */
        write(fd, unique, sizeof(unique));

        string_t *infoline = string_format("%s|%s|%s|%s|%s",
            restinfo->instance,
            restinfo->channel,
            restinfo->user,
            build_date(),
            build_time()
        );

        /*
         * Calculate the length of the info line and store it as the
         * very first thing to the restart file. Then write the info
         * line to the file.
         */
        size_t infosize = string_length(infoline) + 1;
        write(fd, &infosize, sizeof(size_t));
        write(fd, string_contents(infoline), infosize);
        string_destroy(infoline);
        restart_info_destroy(restinfo);

        /* String table will be stored at the end of the file */
        string_t *stringtable = string_construct();

        /* Store number of file descriptors */
        size_t count = list_length(fds);

        write(fd, &count, sizeof(size_t));
        printf("Restart state with %zu network(s):\n", count);

        /* Now deal with the file descriptors */
        list_iterator_t *it = list_iterator_create(fds);
        while (!list_iterator_end(it)) {
            irc_manager_restart_t *e = list_iterator_next(it);
            printf("    Network %s on socket %d stored (%s socket)\n",
                e->name, e->fd, e->ssl ? "secure" : "normal");

            write(fd, &e->fd, sizeof(int));

            /* Apeend to string table seperated by newlines */
            string_catf(stringtable, "%s\n", e->name);

            free(e->data);
            free(e->name);
            free(e);
        }

        /* Write string table */
        write(fd, string_contents(stringtable), string_length(stringtable));
        string_destroy(stringtable);

        /* Unlock for restart */
        redroid_unlock();

        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "-r%d", fd);
        return execv(*argv, (char *[]){ *argv, buffer, (char *)NULL });
    }

    irc_manager_destroy(manager);
    return EXIT_SUCCESS;
}
