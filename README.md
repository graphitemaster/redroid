### Synopsis
Redroid is a modular rewrite of the redroid mIRC bot.

### Features
* __Instances__
    Redroid is designed with modularity in mind, from this feat alone Redroid
    is capable of supporting thousands of instances and thousands of channels
    per instance. Every instance is tied to a database and instances can
    share databases. Instances are configurable from an ini file and from
    the web interface.

* __Modules__
    Redroid comes with support for sandboxed modules. A module is a single
    shared object which is sandboxed by Redroid's sandboxing mechanism making
    modules that are unresponsive or unsafe from affecting the bot. Anyone
    can write a module. Modules themselfs can be loaded, unloaded and even
    reloaded from within IRC as well, making module development increadibly
    painless.

* __Restartable__
    Redroid can be restarted from within IRC by an administrator, or from
    within the web interface without a single instance disconnecting from
    the host. _This currently doesn't work with SSL instances_.

* __Recompilable__
    Redroid can be recompiled from within IRC for rapid development. Couple
    this with restartability and the level of control for module development
    within IRC as well and it makes Redroid the easiest of any IRC bot to
    develop for.

* __Module API__
    Redroid modules can take advantage of the rich module API. A single
    header file API with high level interfaces for IRC, database, sockets,
    random numbers, regular expressions, etc. In addition to the rich API
    provided by Redroid, modules can ignore the tedious work of freeing
    resources thanks to implicit gabrage collection at the API level.

* __Web interface__
    With all the complexity of Redroid, visualizing, configuring and
    communicating with it through configuration files, databases and
    within IRC can seem a bit archaic and tedious. To combat this issue
    Redroid comes with a full fledged web interface which can be accessed
    with standard login credentials from any web browser in the world.
    This same web interface can be used to create additional user accounts
    for other people to administrate the bot as well, making Redroid the
    ideal IRC bot for large communities.

* __Documentation__
    Redroid is heavily documented for people who want to utilize it. This
    is typical of most IRC bots. Redroid is also heavily documented for
    people who want to develop modules for it as well as for people who
    want a better understanding of how it works and what makes it tick.

### Building
Information on the supported build options and prerequisites and how to
build Redroid can be found on the [_Building Redroid_](Building Redroid)
page of our offical wiki.

### Configuration
Information on how to configure Redroid and Redroid modules can be found
on the [_Configuring Redroid_](Configuring Redroid) page of our offical
wiki.

### Miscellaneous
Additional information of Redroid can be found on our [_offical wiki_](Home)

[![Bitdeli Badge](https://d2weczhvl823v0.cloudfront.net/graphitemaster/redroid/trend.png)](https://bitdeli.com/free "Bitdeli Badge")

