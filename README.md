Synopsis
--------
A modular rewrite of the popular mIRC redroid IRC bot:

Features
--------
 * Configuration
     * Instances
 * Instance
     * Multiple networks
        * Multiple channels
     * Multiple nicks
     * Database
     * Modules
 * Flood protection
     * Queued
 * Modules
     * Garbage collected
     * Sandboxed
        * Only whitelisted functions can be used
        * Forecfully shortly lived (3 seconds default)
        * Run in a seperate thread
     * Configurable
     * Module API
         * IRC
         * Database
         * Sockets
         * Regular Expressions
            * Cached
            * POSIX Compatible

Prerequisites
-------------
 * GNU make
 * Linux 2.6+ / FreeBSD 7.0+
 * SQLite3 (-lsqlite3)
 * POSIX Threads (-lpthread)
 * POSIX Realtime Timers (-lrt)
 * POSIX Regular Expressions (regex.h)

Building
--------
Redroid can be built with one simple invocation of GNU make

    $ make

Modules
-------
Included with Redroid are the following modules currently

| Module    | Description                                           |
| --------- | ----------------------------------------------------- |
| cookie    | Chop a target up and make cookies out of the pieces   |
| dance     | Dance like a jolly idiot or give a target a lap dance |
| dns       | Resolve a domain names ipv4 or ipv6 addresses         |
| dur       | Convert seconds to a duration of time                 |
| fail      | Check if a target fails                               |
| family    | Control target family member status                   |
| fnord     | Generate disinformation about a target                |
| gibberish | Generate gibberish text                               |
| help      | Condescendingly reassure target there isn't any help  |
| lava      | Melt target in ball of flames or blindy agony         |
| module    | Load, unload or reload modules                        |
| penish    | Calculate e-penis length of target                    |
| phail     | Reassure target that author of Redroid cannot phail   |
| poetter   | Shit Lennart Poettering says                          |
| quit      | Shutdown Redroid                                      |
| quote     | Quote database control                                |
| twss      | That's what she said monitor                          |

You can write your own modules using the `module.h` header in `modules/`.
Compiling your module with `make` is also possible as long as your module
is inside the `modules/` directory.

You can also load newely created modules while the bot is already running
by invoking the `module` module from IRC with the bot pattern using the
arguments `-load [modulename]`.

Documentation for modules can be procured by invoking the bot from IRC
using the argument `-help`. Your module should contain handling for such
an argument as a result to stay consistent with Redroid as a whole.

Running
-------
Setup the appropriate instance(s) inside config.ini then invoke
the bot directly with

    $ ./redroid
    $ ./redroid -q        # run quiet
    $ ./redroid -d        # run as daemon
    $ ./redroid -l <file> # log to <file>

You can use CTRL+C at anytime to send a shutdown signal, which will
safely shut down the process.

In daemon mode there is no easy way to shut down the bot other than
to send a `SIGUSR1` signal with kill, for that you'll need the PID.
Optionally you can `killall -SIGUSR1 redroid`.

It isn't very probable or often; since modules are run in a seperate
thread, but if commands stop working you can forcefully restart the
command processor by sending `SIGUSR2` to the process.

If you invoke redroid in a terminal in normal/quiet mode; closing the
terminal will force daemonization of the redroid process. Similarly
this can be done by sending `SIGHUP` to the process.

A full list of the signals Redroid intercepts and what they do is
provided below for reference.

| Signal   | Reason                    | Action                         |
| -------- | ------------------------- | ------------------------------ |
| SIGUSR1  | Internal error            | Quick Shutdown                 |
| SIGUSR2  | Command processor restart | Restarts the command processor |
| SIGSEGV  | Command processor restart | Restarts the command processor |
| SIGHUP   | Deamonization             | Deamonizes Redroid             |
| SIGINT   | Shutdown                  | Clean Shutdown                 |
| SIGTERM  | Shutdown                  | Clean Shutdown                 |
| SIGALARM | Command processor timeout | Raises SIGUSR2 on timeout      |
