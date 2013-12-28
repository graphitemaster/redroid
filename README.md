Synopsis
--------
A modular rewrite of the popular mIRC redroid IRC bot:

Features
--------
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

Prerequisites
-------------
 * GNU make
 * Linux 2.6+/BSD
 * SQLite3 (-lsqlite3)
 * POSIX Threads (-lpthread)

Building
--------
Redroid can be built with one simple invocation of GNU make

    $ make

Running
-------
Setup the appropriate instance(s) inside config.ini then invoke
the bot directly with

    $ ./redroid
    $ ./redroid -q # run quiet
    $ ./redroid -d # run as daemon

You can use CTRL+C at anytime to send a shutdown signal, which will
safely shut down the process.

In daemon mode there is no safe way to shut down the bot other than
to send a SIGINT with kill, for that you'll need the PID. Optionally
you can `killall -3 redroid`.

If you invoke redroid in a terminal in normal/quiet mode. Closing the
terminal will force daemonization of the redroid process (SIGHUP).

Architecture
------------

A description of the architecture behind redroid is provided in
ARCHITECTURE for developers, which describes the architecture behind
redroid in a high-enough-level fashion to help developers to get
familiarized with the source code.
