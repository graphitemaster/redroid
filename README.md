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
     * SSL
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
            * Cached
         * Sockets
         * Random numbers
            * Mersenne Twister (MT1997)
         * Regular Expressions
            * Cached
            * POSIX Compatible
     * SVN
         * Query remote SVN logs

Prerequisites
-------------
 * GNU make
 * Linux 2.6+ / FreeBSD 7.0+
 * SQLite3 (-lsqlite3)
 * POSIX Threads (-lpthread)
 * POSIX Realtime Timers (-lrt)
 * POSIX Regular Expressions (regex.h)
 * Subversion (svn command line tool)
 * OpenSSL (-lssl and -lcrypto)

Building
--------
Redroid can be built with one simple invocation of GNU make:

    $ make

You can also disable features by invoking `configure`. For instance to
build without SSL support, try:

    $ ./configure --disable-ssl
    $ make

A list of all the options for the configure script can be obtained
like so:

    $ ./configure --help

Modules
-------
Currently included with Redroid are the following modules:

| Module                                                                    | Description                                           |
| ------------------------------------------------------------------------- | ----------------------------------------------------- |
| [calc](https://github.com/graphitemaster/redroid/wiki/mod_calc)           | Infix calculator with constants and functions         |
| [cookie](https://github.com/graphitemaster/redroid/wiki/mod_cookie)       | Chop a target up and make cookies out of the pieces   |
| [dance](https://github.com/graphitemaster/redroid/wiki/mod_dance)         | Dance like a jolly idiot or give a target a lap dance |
| [dns](https://github.com/graphitemaster/redroid/wiki/mod_dns)             | Resolve a domain name's IPv4 or IPv6 addresses        |
| [dur](https://github.com/graphitemaster/redroid/wiki/mod_dur)             | Convert seconds to a duration of time                 |
| [fail](https://github.com/graphitemaster/redroid/wiki/mod_fail)           | Check if a target fails                               |
| [family](https://github.com/graphitemaster/redroid/wiki/mod_family)       | Control target family member status                   |
| [faq](https://github.com/graphitemaster/redroid/wiki/mod_faq)             | Frequently answered questions                         |
| [fnord](https://github.com/graphitemaster/redroid/wiki/mod_fnord)         | Generate disinformation about a target                |
| [gibberish](https://github.com/graphitemaster/redroid/wiki/mod_gibberish) | Generate gibberish text                               |
| [help](https://github.com/graphitemaster/redroid/wiki/mod_help)           | Condescendingly reassure target there isn't any help  |
| [kitten](https://github.com/graphitemaster/redroid/wiki/mod_kitten)       | Play with the lives of kittens in the name of n00bs   |
| [lava](https://github.com/graphitemaster/redroid/wiki/mod_lava)           | Submerge target in deadly lava                        |
| [module](https://github.com/graphitemaster/redroid/wiki/mod_module)       | Load, unload or reload modules                        |
| [obit](https://github.com/graphitemaster/redroid/wiki/mod_obit)           | Generate an obituary of someone or yourself           |
| [penish](https://github.com/graphitemaster/redroid/wiki/mod_penish)       | Calculate e-penis length of target                    |
| [phail](https://github.com/graphitemaster/redroid/wiki/mod_phail)         | Reassure target that author of Redroid cannot phail   |
| [poetter](https://github.com/graphitemaster/redroid/wiki/mod_poetter)     | Shit Lennart Poettering says                          |
| [quote](https://github.com/graphitemaster/redroid/wiki/mod_quote)         | Quote database control                                |
| [svn](https://github.com/graphitemaster/redroid/wiki/mod_svn)             | Subversion commit notifier                            |
| [system](https://github.com/graphitemaster/redroid/wiki/mod_system)       | Restart or get information about Redroid              |
| [twss](https://github.com/graphitemaster/redroid/wiki/mod_twss)           | That's what she said monitor                          |
| [youtube](https://github.com/graphitemaster/redroid/wiki/mod_youtube)     | YouTube link monitor                                  |

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
to send a `SIGINT` signal with kill, for that you'll need the PID.
Optionally you can `killall -SIGINT redroid`.


[![Bitdeli Badge](https://d2weczhvl823v0.cloudfront.net/graphitemaster/redroid/trend.png)](https://bitdeli.com/free "Bitdeli Badge")

