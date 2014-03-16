### Synopsis
Redroid is a modular rewrite of the redroid mIRC bot.

### Features
* __Instances__
    Redroid is designed with modularity in mind. Due to this feat
    alone, Redroid is capable of supporting thousands of instances and
    thousands of channels per instance. Every instance is tied to a database
    and instances can share databases. Instances are configurable via either an
    INI file or through the web interface.

* __Modules__
    Redroid comes with support for sandboxed modules. A module is a single
    shared object which is sandboxed by Redroid's sandboxing mechanism making
    modules that are unresponsive or unsafe from affecting the bot. Anyone
    can write a module. Modules themselves can be loaded, unloaded and even
    reloaded from within IRC, making module development incredibly quick and
    easy. Redroid comes with a plethora of already written, tested and
    configurable modules. A list of these modules can be found on the
    [_Modules_](Modules) article of our official wiki.

* __Restartable__
    Redroid can be restarted from within IRC by an administrator, or from
    within the web interface without a single instance disconnecting from
    the host. _This currently doesn't work with SSL instances_.

* __Recompilable__
    Redroid can be recompiled from within IRC for rapid development. Couple
    this with restartability and the level of control for module development
    within IRC as well, and it makes Redroid the easiest of any IRC bot to
    develop for.

* __Module API__
    Redroid modules can take advantage of the rich module API, a single
    header file API with high level interfaces for IRC, database, sockets,
    random numbers, regular expressions, and so on. In addition to the rich API
    provided by Redroid, modules can ignore the tedious work of freeing
    resources thanks to implicit garbage collection at the API level.

* __Web interface__
    With all the complexity of Redroid, visualizing, configuring and
    communicating with it through configuration files, databases and
    within IRC can seem a bit archaic and tedious. To combat this issue,
    Redroid comes with a fully-fledged web interface that can be accessed
    with standard login credentials from any web browser in the world.
    This same web interface can be used to create additional user accounts
    for other people to administrate the bot as well, making Redroid the
    ideal IRC bot for large communities.

* __Documentation__
    Redroid is thoroughly documented for people who want to utilize it. This
    is not typical of most IRC bots. Redroid is also heavily documented for
    people who want to develop modules for it as well as for people who
    want a better understanding of how it works and what makes it tick, which
    gives Redroid a great edge over most of its competition.

### Building
Information on the supported build options, prerequisites, and on how to
build Redroid can be found on the [_Building Redroid_](Building Redroid)
article of our official wiki.

### Configuration
Information on how to configure Redroid and its modules can be found
on the [_Configuring Redroid_](Configuring Redroid) article of our official
wiki.

### Miscellaneous
Additional information of Redroid can be found on our [_official wiki_](Home).

[![Bitdeli Badge](https://d2weczhvl823v0.cloudfront.net/graphitemaster/redroid/trend.png)](https://bitdeli.com/free "Bitdeli Badge")
