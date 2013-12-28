The architecture of redroid:

Configuration
-------------

Redroid IRC instances are specified by a configuration file in the form
of an INI file currently. There are future prospects of using an SQLITE
database for this or possibly a YAML file due to the inflexibility imposed
on INI files. An INI file currently describes IRC instances for redroid
to create when run. An IRC instance is simply a sectional INI entry
containing information about the IRC server, port, which nick the bot
should use on that network, etc. You can define multiple IRC instances
within the configuration file, all of which will be created and managed
by the IRC manager once succesfully loaded.

IRC manager
-----------

The IRC manager is a small round-robin scheduler that currently runs on
a single thread. Its primary job is to invoke the appropriate non-blocking
IRC process for all IRC instances it contains. To acomplish this a simple
list of IRC instances and a cached iterator are used (for reentrancy).
Algorithmically it's just:
```
    for (instance = cached_iterator->data; instance; cached_iterator = instance->next)
        irc_non_blocking_process(instance);
```

The IRC manager is also responsible for managing the command processor,
and message channel.

IRC process
-----------

The IRC process is a single IRC instance which is run by the IRC manager's
scheduler. Due to the nature of the IRC manager the IRC process needs to
be non-blocking otherwise the IRC manager would be incapable of updating
other IRC instances within its list. The IRC process is given the IRC
managers message channel, such that the IRC process can create messages
for the command processor. This is acomplished by parsing the contents
of a PRIVMSG and determining if the contents match the ruleset for that
module. The IRC process is also responsible for joining channels, rejoining
on kick and registering with ircd authorithies like ChanServ, as well as handling
keep-alive ping-pong with the server. In addition to what it has to manage
the IRC process is also responsible for managing a thread-safe queue of
messages (actions and private messages). Modules are capable of writing
out to channels/users etc and to ensure that modules don't flood the network
a queue is used to do flood protection, amongst other things.

Command processor
-----------------

The Command processor is the core of managing the messages recieved from
the IRC process and running the appropriate module associated with that
message. The command processor itself is threaded, in that it runs modules
asynchronous from the IRC process. To acomplish this a thread-safe channel
is used, where the IRC process writes messages to, and the command processors
single thread reads from. Once a valid entry from the queue is dequeued by
the worker thread, the appropriate module entry point is called for it. However
before doing so the current time is recorded. The IRC manager calls
a seperate non-blocking command update loop to read this recorded time to
determine if a module has been running for too long, when that is determined
the message is thrown away and the thread is restarted. This roboust design
leads to some troubling issues regarding resource management. If a module
times out, there is no way to know what has been allocated and hasn't be
freed. Leading to a huge potential for memory leaks. To comabd this the
command processor contains a linear-list-garbage collector.

Module garbage collector
------------------------

The garbage collector isn't necessarily a garbage collector in the traditional
sense, but rather a list of pointers of allocated resources from a module and
the appropriate pointer-to-destroy-function for that memory. When a module
allocates resources, behind the scene the pointer and appropriate destroy
callback is added to a thread-safe list. When the command processor completes
a successful call to a module (in that it returns), or if the thread in which
it was running is restarted, the contents of the list are iterated and all
the appropriate callback functions (with the associated data passed in) are
called. This effectively solves the issue with managing resources in modules.
Due to the nature of how modules are shortly-lived to begin with and this
garbage collection mechanism, there is no requirement to explicitly free
resources in modules either, leading to simplier more managable modules.

Modules
-------

As already described, modules shouldn't try to free resources because there
is a garbage collector. However there is a requirement that the appropriate
functions are used in modules to ensure that the function which allocates
resources is pinned to the garbage collector. This is acomplished with modules
getting their own specific API. For instance a module cannot call malloc, but
it can call module_malloc, which as an additional argument take a pointer
to the module handle. The reason for this is to ensure that the memory of the
behind the scene malloc is actually pinned to the garbage collector (with the
appropriate destroy function, in this case free). This would normally be fine
except that people will mistakingly use malloc, or another resource allocating
function. Which brings us to the first part of modules, mainly disallowing
those functions from being used.

White/Black listing functions: Since a module is just a shared object file
by definition it is quite trivial to parse the list of external symbols it
uses, even more so in the case of ELFs. The first stage of module loading
the module .dynsym section is checked against a whitelist of functions a
module is allowed to invoke. If a function is found which isn't in the
whitelist the module is declined with an appropriate error message to
indicate why.

The final stage to module loading is the appropriate loading of the module,
mainly searching for the entry/exit symbols with dlsym, and for the ruleset
of what command to match. Modules define their intent with a macro provided
to them by <module.h> which has to be included for a module to even load.
This header has two macros: MODULE_DEFAULT, and MODULE_ALWAYS. Depending
on the module attempt, using MODULE_ALWAYS(module_name) will ensure that
the modules entry function is entered always for any message on any channel.
This is useful for writing a module that needs to check if someone posted
a link to a webpage for instance. Where as MODULE_DEFAULT(module_name) is
used to indicate a command-like module whos entry function is only called
when a user types [bot_pattern][module_name]. For instance ~module_name
in the case of the default bot pattern.

Since modules are just shared objects they can be reloaded, or loaded into
an IRC instance on the fly. The modules ~load and ~reload are examples of
modules which implement load/reload of modules. Yes a module which implements
the ability to load, reload and unload modules is possible. This makes it
quite trivial to roll out module changes into the wild without having to
restart the IRC instance that is in use, or the whole bot.

Database
--------

Redroid also contains a small wrapper database around SQLITE useful for
modules to use, since it's properly garbage collected and easy to use.
An example of a module which uses it (~quote) is provided. In the future
a lot more things will use it (~family, ~oracle, ~obit). At the basic form
the database is intended to be used as such:
```
    database_statement_t *statement = database_statement_create(module, "PREPARED_STATEMENT");
    if (!statement) return;

    int i = 0;
    char *s = "hi";

    // s = string
    // i = int
    // b = binary (second argument needs to be size)
    if (!database_statement_bind(statement, "si", i, s))
        return;

    // extract one row:
    // the order is important, e.g if the row cotnains
    // | INT | STRING |
    // then you use "si"
    database_row_t *get = database_row_extract(statement, "i");
    if (!get)
        return;

    int pop = database_row_pop_integer(get);

    printf("%d\n", pop); // print value in database

    // no need to cleanup in modules, but you can test to see if the
    // operation WAS successful with

    if (!(database_statement_complete(statement))
        ; // not sucessfull
```
