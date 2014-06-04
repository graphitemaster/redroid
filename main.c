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

#define RESTART_FILENAME  "redroid_XXXXXX"
#define RESTART_FILESIZE  sizeof(RESTART_FILENAME)
#define RESTART_MAGICDATA "Redroid"
#define RESTART_MAGICSIZE sizeof(RESTART_MAGICDATA)

extern const char *build_info(void);

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
    (void)channel; /* ignored */
    (void)user; /* ignored */
    /* todo print message */
    redroid_shutdown_global(irc->manager);
}

typedef enum {
    DAEMONIZATION_REDUNDANT,
    DAEMONIZATION_FAILED,
    DAEMONIZATION_SUCCESSFUL
} daemon_status_t;

static daemon_status_t redroid_daemon(bool closehandles) {
    static bool daemonized = false;
    if (daemonized)
        return DAEMONIZATION_REDUNDANT;

    pid_t sid;
    if ((sid = setsid()) == -1)
        return DAEMONIZATION_FAILED;

    pid_t pid = getppid();

    /*
     * Child's parent becomes init process which means we cannot daemonize
     * as we're already daemonized.
     */
    if (pid == 1)
        return DAEMONIZATION_REDUNDANT;

    if (kill(pid, SIGINT) == -1)
        return DAEMONIZATION_FAILED;

    if (closehandles) {
        (void)!freopen("/dev/null", "w", stdout);
        (void)!freopen("/dev/null", "w", stderr);
    }

    daemonized = true;
    return DAEMONIZATION_SUCCESSFUL;
}

void redroid_daemonize(irc_t *irc, const char *channel, const char *user) {
    daemon_status_t status = redroid_daemon(true);
    switch (status) {
        case DAEMONIZATION_SUCCESSFUL:
            irc_write(irc, channel, "%s: daemonization successful", user);
            break;
        case DAEMONIZATION_REDUNDANT:
            irc_write(irc, channel, "%s: already daemonized", user);
            break;
        case DAEMONIZATION_FAILED:
            irc_write(irc, channel, "%s: daemonization failed", user);
            break;
    }
    irc_manager_wake(irc->manager);
}

void redroid_abort(void) {
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
        goto redroid_recompile_fail_file;
    }

    fclose(fp);

    backupname = string_format("%s.bak", redroid_binary);
    if (!(fp = fopen(string_contents(backupname), "wb"))) {
        error = "backing up redroid binary (failed creating backup file)";
        goto redroid_recompile_fail_file;
    }

    if (fwrite(binread, binsize, 1, fp) != 1) {
        error = "backing up redroid binary (failed writing backup file)";
        goto redroid_recompile_fail_file;
    }

    /* Make sure the binary can be executed */
    if (chmod(string_contents(backupname), S_IRWXU) != 0) {
        error = "backing up redroid binary (failed setting executable permissions for backup file)";
        goto redroid_recompile_fail_file;
    }

    fclose(fp);

    if (!(fp = popen("make clean", "r"))) {
        error = strerror(errno);
        goto redroid_recompile_fail;
    }

    if (pclose(fp) == -1) {
        error = strerror(errno);
        goto redroid_recompile_fail;
    }

    /* Now try the recompile */
    if (!(fp = popen("make 2>&1", "r"))) {
        error = strerror(errno);
        goto redroid_recompile_fail;
    }

    list_t *lines = list_create();
    char   *line  = NULL;
    size_t  size  = 0;

    while (getline(&line, &size, fp) != EOF)
        list_push(lines, strdup(line));
    free(line);

    int tryclose = pclose(fp);
    if (tryclose == -1)
        irc_write(irc, channel, "%s: recompiled failed (%s)\n", user, strerror(errno));
    else if (tryclose != 0) {
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
        list_foreach(errors, NULL, &free);
        list_destroy(errors);
        rename(string_contents(backupname), redroid_binary);
    } else {
        irc_write(irc, channel, "%s: recompiled successfully", user);
        redroid_restart(irc, channel, user);
        remove(string_contents(backupname));
    }

    string_destroy(backupname);
    list_foreach(lines, NULL, &free);
    list_destroy(lines);
    return;

redroid_recompile_fail_file:
    if (fp)
        fclose(fp);
redroid_recompile_fail:
    if (backupname)
        string_destroy(backupname);

    irc_write(irc, channel, "%s: failed recompiling", user);
    irc_write(irc, channel, "%s: %s", user, error);
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

/* The only true global */
static irc_manager_t *manager = NULL;

static void signal_handle_parent(int sig) {
    (void)sig;
    exit(EXIT_SUCCESS);
}

static void signal_handle(int sig) {
    if (sig == SIGTERM || sig == SIGINT)
        printf("Received shutdown\n");
    else
        printf("Received internal error\n");
    printf("Shutting down ...\n");
    irc_manager_wake(manager);
    signal_shutdown(true);
    fflush(NULL);
}

static void signal_install(void) {
    signal(SIGTERM, &signal_handle);
    signal(SIGINT,  &signal_handle);
}

static bool signal_restarted(int *argc, char ***argv, int *tmpfd) {
    if (*argc != 2 || strncmp((*argv)[1], "-r", 2))
        return false;
    if (sscanf(&((*argv)[1][2]), "%d", tmpfd) != 1)
        return false;

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
    /*
     * Okay a little explination what we're doing here.
     *  There is no sensible way to 'detach' a parent process from the
     *  underlying terminal which launched the process because it's
     *  waitpid()'n on it. This means there is really no clean way to
     *  do daemonization within the middle of process execution without
     *  restarting the entire process.
     *
     *  How this works is quite simple. We fork and run the entire
     *  bot within the child process letting the parent do nothing but
     *  pause. When we get a deamonization request we can simply setsid
     *  within the child to get a new session and kill the parent.
     *
     *  This gives the underlying shell which invoked the process the
     *  status it wants from waitpid while the process can continue to
     *  run as a child which has now been promoted to the controlling
     *  session leader.
     */
    pid_t pid = fork();
    if (pid != 0) {
        signal(SIGINT, &signal_handle_parent);
        int status;
        waitpid(pid, &status, 0);
        _exit(status);
    }

    /* Run the entire thing as a child process */
    web_t *web = NULL;

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

    int tmpfd = -1;
    if (signal_restarted(&argc, &argv, &tmpfd)) {
        if (lseek(tmpfd, RESTART_MAGICSIZE, SEEK_SET) != RESTART_MAGICSIZE) {
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
        char *info     = strdup(strtok(NULL,     "|"));

        /* no suitable restart information when coming from web client */
        if (strcmp(instance, ";webclient")) {
            irc_t *update = irc_manager_find(manager, instance);
            irc_write(update, channel, "%s: successfully restarted", user);

            if (strcmp(info, build_info())) {
                irc_write(update, channel, "%s: last instance %s", user, info);
                irc_write(update, channel, "%s: this instance %s", user, build_info());
            } else {
                irc_write(update, channel, "%s: this instance is the same binary as last instance", user);
            }
        } else {
            /* broadcast to all channels and servers */
            irc_manager_broadcast(manager, "successfully restarted");
            if (strcmp(info, build_info())) {
                irc_manager_broadcast(manager, "last instance %s", info);
                irc_manager_broadcast(manager, "this instance %s", build_info());
            } else {
                irc_manager_broadcast(manager, "this instance is the same binary as last instance");
            }
        }

        unlink(tmpfilename);

        free(info);
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
                    (void)!freopen(&argv[0][3], "w", stdout);
                    (void)!freopen(&argv[0][3], "w", stderr);
                    redroid_daemon(false);
                    break;

                case 'd': /* Daemonize */
                    redroid_daemon(true);
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

    web_begin(web, manager);

    while (signal_empty())
        irc_manager_process(manager);

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

        string_t *infoline = string_format("%s|%s|%s|%s",
            restinfo->instance,
            restinfo->channel,
            restinfo->user,
            build_info()
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

        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "-r%d", fd);
        return execv(*argv, (char *[]){ *argv, buffer, (char *)NULL });
    }

    irc_manager_destroy(manager);
    return EXIT_SUCCESS;
}
