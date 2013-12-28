A modular rewrite of the popular mIRC redroid IRC bot:

Features:

    * Configuration
        * Instances
    * Multithreaded
    * Multiple networks
        * Multiple channels
    * Flood protection
        * Queued
    * Modules
        * Garbage collected
        * Sandboxed
        * Configurable
        * Module API
            * IRC
            * Database
            * Sockets

Prerequisites:

    * GNU make
    * Linux 2.6+/BSD
    * SQLite3 (-lsqlite3)
    * POSIX Threads (-lpthread)

Building:

    $ make

Running:

    Setup the appropriate instance(s) inside config.ini then invoke
    the bot directly with

    $ ./redroid

    You can use CTRL+C at anytime to send a shutdown signal, which will
    safely shut down the process.

    Optionally you can run it quietly

    $ ./redroid -q

    CTRL+C is also valid to send the shutdown signal when running with
    the quiet switch.

    Or you can run it as a daemon.

    $ ./redroid -d

    In daemon mode there is no safe way to shut down the bot other than
    to send a SIGINT with kill, for that you'll need the PID. Optionally
    you can killall -3 redroid.

    If you invoke redroid in a terminal in normal/quiet mode. Closing the
    terminal will force daemonization of the redroid process (SIGHUP).

Architecture:

    A description of the architecture behind redroid is provided in
    ARCHITECTURE for developers, which describes the architecture behind
    redroid in a high-enough-level fashion to help developers to get
    familiarized with the source code.
