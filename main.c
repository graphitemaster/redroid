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

    switch (signal) {
        case SIGTERM:
        case SIGINT:
            printf("Recieved shutdown signal\n");
            handler = &signal_shutdown;
            message = "Shutting down";
            break;

        case SIGUSR1:
            printf("Recieved internal error\n");
            handler = &signal_shutdown;
            message = "Shutting down";
            break;

        case SIGCHLD:
            printf("Recieved restart signal\n");
            handler = &signal_restart;
            message = "Restarting";
            break;

        case SIGHUP:
            return signal_daemonize(true);

        default:
            raise(SIGUSR1);
            return;
    }

    if (!handler) {
        raise(SIGUSR1);
        return;
    }

    handler(true);
    printf("%s ...\n", message);
    fflush(NULL);

}

static void signal_install(void) {
    signal(SIGUSR1, &signal_handle);
    signal(SIGTERM, &signal_handle);
    signal(SIGINT,  &signal_handle);
    signal(SIGHUP,  &signal_handle);
    signal(SIGCHLD, &signal_handle);
}

static bool signal_restarted(int *argc, char ***argv, int *memhandle) {
    /*
     * To determine if we restarted argv will contain a -r flag with
     * the next argument containing the file descriptor for the shared
     * memory file descriptor preceeded by everything else.
     */
    if (!strncmp(**argv, "-r", 2)) {
        *memhandle = shm_open("redroid", O_RDWR, 0666);
        (*argc)--;
        (*argv)++;
        return true;
    }
    return false;
}

int main(int argc, char **argv) {
    irc_manager_t *manager = NULL;
    signal_install();
    srand(time(0));

    int shmem;
    if (signal_restarted(&argc, &argv, &shmem)) {
        /*
         * If the process was restarted then argc and argv will be what
         * they would typically be had it not been a restarted process.
         * We'll also get a handle to a piece of shared memory from the
         * process just before it was restarted which contains in it
         * all the information we need to reinstate everything as if the
         * process never stoped to begin with.
         */
        struct stat buf;
        if (fstat(shmem, &buf) != 0) {
            fprintf(stderr, "%s: restart failed (reason: fstat failed) [%s]\n", *argv, strerror(errno));
            return EXIT_FAILURE;
        }
        void *memory = mmap(0, buf.st_size, PROT_READ, MAP_SHARED, shmem, 0);
        if (memory == MAP_FAILED) {
            fprintf(stderr, "%s: restart failed (reason: mmap failed) [%s]\n", *argv, strerror(errno));
            return EXIT_FAILURE;
        }

        printf("Restarted (%d descriptor(s))\n", *(int*)memory);

        // TODO: actual restart by using that information to repopulate
        // stuff but it's good we made it here.

        /*
         * Now free the resources for that shared memory as we're finished
         * with it.
         */
        if (munmap(memory, buf.st_size) != 0)
            fprintf(stderr, "%s: restart warning (failed to release shared memory) continuing with leak of %zu bytes\n", *argv, buf.st_size);
        shm_unlink("redroid");
    }

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

    if (!(manager = irc_manager_create())) {
        fprintf(stderr, "failed creating irc manager\n");
        return EXIT_FAILURE;
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

    if (irc_manager_empty(manager)) {
        fprintf(stderr, "No IRC instances to manage\n");
        raise(SIGINT);
    }

    while (signal_empty())
        irc_manager_process(manager);

    if (!signal_restart(false)) {
        /*
         * Destroy the existing irc instances by save the file descriptor
         * list and the networks and channels it was on and then stuff
         * that into some shared memory. We will then re-execute this
         * process with -r<shmemfd> as the very first argument so that
         * the process_restarted logic can reinstate this process as
         * if nothing ever happened.
         */
        list_t *list = irc_manager_restart(manager);
        int     fd   = shm_open("redroid", O_CREAT | O_TRUNC | O_RDWR, 0666);
        size_t  size = list_length(list);

        /*
         * shm_open does implicit FD_CLOEXEC. This is no good because
         * the descriptor is lost once we execv.
         */
        int flags = fcntl(fd, F_GETFD);
        flags &= ~FD_CLOEXEC;
        fcntl(fd, F_SETFD, flags);

        if (fd == -1) {
            fprintf(stderr, "%s: restart failed (reason: shm_open failed) [%s]\n", *argv, strerror(errno));
            list_destroy(list);
            return EXIT_FAILURE;
        }

        /*
         * Round the required memory to the nearest page size for mmap
         * since that is required.
         */
        const size_t pagesize = sysconf(_SC_PAGE_SIZE);
        size = ((size + pagesize - 1) / pagesize) * pagesize;

        /*
         * Truncate the shared memory request to that size before we
         * get a pointer to it via mmap.
         */
        int truncate = ftruncate(fd, size);
        if (truncate != 0) {
            shm_unlink("redroid");
            list_destroy(list);
            fprintf(stderr, "%s: restart failed (reason: ftruncate failed) [%s]\n", *argv, strerror(errno));
            return EXIT_FAILURE;
        }

        void *memory = mmap(0, pagesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (memory == MAP_FAILED) {
            shm_unlink("redroid");
            list_destroy(list);
            fprintf(stderr, "%s: restart failed (reason: mmap failed) [%s]\n", *argv, strerror(errno));
            return EXIT_FAILURE;
        }

        /*
         * Now iterate the list storing off literally every single file
         * descriptor. The first sizeof(size_t) of our memory will contain
         * the size of the shared memory size itself for the later calls
         * to mmap/munmap.
         */
        int *store = memory;

        /*
         * Store the number of fds first. list_length will return
         * size_t so this breaks the interface contract but we assume
         * that > INT_MAX fds are not possible to begin with.
         */
        *store++ = (int)list_length(list);

        list_iterator_t *it = list_iterator_create(list);
        while (!list_iterator_end(it)) {
            void *ptr = list_iterator_next(it);
            *store++ = (union { void *p; int i; }){ ptr }.i;
        }

        execv(*argv, (char *[]){ "-r", *argv, (char *)NULL });
    }

    irc_manager_destroy(manager);
    return EXIT_SUCCESS;
}
