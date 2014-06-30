#ifndef REDROID_MODULE_MODULE_HDR
#define REDROID_MODULE_MODULE_HDR

#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

#ifdef DOXYGEN_SHOULD_SKIP_THIS
    /**
     * @brief Mark the module as `default'.
     *
     * Default modules are modules which are executed on command.
     *
     * @param NAME The name of the module.
     */
#   define MODULE_DEFAULT(NAME)

    /**
     * @brief Mark the module as `always'.
     *
     * Always modules are modules which are executed always.
     *
     * @param NAME The name of the module.
     */
#   define MODULE_ALWAYS(NAME)

    /**
     * @brief Mark the module as `interval'.
     *
     * Interval modules are modules which execute at a given interval.
     *
     * @param NAME      The name of the module.
     * @param INTERVAL  The interval (in seconds) in which to execute this module.
     */
#   define MODULE_TIMED(NAME, INTERVAL)
#else
#   define MODULE_GENERIC(NAME, MATCH) \
        char module_name[] = NAME, module_match[] = MATCH
#   define MODULE_DEFAULT(NAME) \
        MODULE_GENERIC(#NAME, #NAME)
#   define MODULE_ALWAYS(NAME) \
        MODULE_GENERIC(#NAME, "")
#   define MODULE_TIMED(NAME, IVAL) \
        MODULE_GENERIC(#NAME, "");  \
        int module_interval = IVAL
#endif

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define MODULE_API_CALL(NAME) \
        ({ extern __typeof__(NAME) module_api_##NAME; &module_api_##NAME; })
#   define MODULE_LIBC_CALL(NAME) \
        ({ extern __typeof__(NAME) module_libc_##NAME; &module_libc_##NAME; })
#endif

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define MODULE_API static inline
#else
#   define MODULE_API
#endif

/** @} */

/** @defgroup List
 *
 * An efficent doubly-linked list implementation.
 *
 * @{
 */

/**
 * @brief A list.
 *
 * An opaque type for representing a list.
 */
typedef struct list_s list_t;

/**
 * @brief Create a list.
 *
 * Creates an empty list.
 *
 * @returns
 * A new empty list.
 */
MODULE_API list_t *list_create(void) {
    return MODULE_API_CALL(list_create)();
}

/**
 * @brief Pop element off list.
 *
 * Pops an element off the tail of the list.
 *
 * @param list          The list to pop element off of.
 *
 * @returns
 * The value at the tail of the list.
 */
MODULE_API void *list_pop(list_t *list) {
    return MODULE_API_CALL(list_pop)(list);
}

/**
 * @brief Shift element off list.
 *
 * Shifts an element off the head of the list.
 *
 * @param list          The list to shift element off of.
 *
 * @returns
 * The value at the head of the list.
 */
MODULE_API void *list_shift(list_t *list) {
    return MODULE_API_CALL(list_shift)(list);
}

/**
 * @brief Get the length of a list.
 *
 * Get the amount of elements the list contains.
 *
 * @param list          The list to get the length of.
 *
 * @returns
 * The amount of elements the list contains.
 */
MODULE_API size_t list_length(list_t *list) {
    return MODULE_API_CALL(list_length)(list);
}

/**
 * @brief Get an element at an index in the list.
 *
 * Get an element in a list at a specified index. This will give you the *index'th*
 * element in the list following *next* pointers *index* times. This function is
 * optimized with the use of a list node caching mechanism that tries to maintain
 * a vector of list nodes along side the list in the same order of the list. The best
 * case for this function is O(1) armortized as a result.
 *
 * @param list          The list to get an element from.
 * @param index         The index of the element in the list.
 *
 * @returns
 * The element at *index* in the list or NULL otherwise.
 */
MODULE_API void *list_at(list_t *list, size_t index) {
    return MODULE_API_CALL(list_at)(list, index);
}

/**
 * @brief Push an element on the list.
 *
 * Appends an element to the list.
 *
 * @param list          The list to append to.
 * @param element       The element to append.
 */
MODULE_API void list_push(list_t *list, void *element) {
    return MODULE_API_CALL(list_push)(list, element);
}

#ifdef DOXYGEN_SHOULD_SKIP_THIS
/**
 * @brief Sort a list
 *
 * Sorts a list using a predicate. The predicate takes on the form bool(T *, T*),
 * T can be any type.
 *
 * @param list          The list to sort.
 * @param predicate     The predicate (comparison function) callback to sort the list.
 *
 */
void list_sort(list_t *list, T predicate);

/**
 * @brief Iterate over a list with a callback.
 *
 * Iterates over a list with a callback. The callback takes on the form void(T *, T*),
 * T can be any type. Optionally the callback can take on the form void(T *) if
 * *pass* == NULL.
 *
 * @param list          The list to iterate through.
 * @param pass          Something to pass as the second argument for the callback.
 * @param callback      he callback function to execute on each iteration.
 *
 */
void list_foreach(list_t *list, void *pass, T callback);

/**
 * @brief Search for something in a list.
 *
 * Searches for something in a list using a predicate callback. The callback
 * takes on the form bool(T *, T *). Optionally the predicate can take on the
 * form bool(T *) if *pass* == NULL.
 *
 * The predicate function must return true to indicate a match, false otherwise.
 * The first argument of the predicate function is the value of the current search
 * iteration.
 *
 * @param list          The list to search in.
 * @param pass          Something to pass as the second argument for the predicate.
 * @param predicate     The predicate function to execute when searching.
 *
 * @returns
 * The value in the list when found, NULL otherwise.
 */
void *list_search(list_t *list, const void *pass, T predicate);
#else
MODULE_API void list_sort_impl(list_t *list, bool (*predicate)(const void *, const void *)) {
    return MODULE_API_CALL(list_sort_impl)(list, predicate);
}

MODULE_API void list_foreach_impl(list_t *list, void *pass, void (*callback)(void *, void *)) {
    return MODULE_API_CALL(list_foreach_impl)(list, pass, callback);
}

MODULE_API void *list_search_impl(list_t *list, const void *pass, bool (*predicate)(const void *, const void *)) {
    return MODULE_API_CALL(list_search_impl)(list, pass, predicate);
}

#define list_sort(LIST, PRDICATE) \
    list_sort_impl((LIST), ((bool (*)(const void *, const void *))(PREDICATE)))

#define list_foreach(LIST, PASS, CALLBACK) \
    list_foreach_impl((LIST), (PASS), ((void (*)(void *, void *))(CALLBACK)))

#define list_search(LIST, PASS, PREDICATE) \
    list_search_impl((LIST), (PASS), ((bool (*)(const void *, const void *))(PREDICATE)))
#endif /*! DOXYGEN_SHOULD_SKIP_THIS */

/** @} */

/**
 * @defgroup String
 *
 * A managed string object.
 *
 * @{
 */

/**
 * @brief A string.
 *
 * An opaque type for representing a managed string.
 */
typedef struct string_s string_t;

/**
 * @brief Create a string.
 *
 * Constructs a managed string object from a C-style string.
 *
 * @param input         String to construct from.
 *
 * @returns
 * A managed string object of the same contents of the C-style string.
 */
MODULE_API string_t *string_create(const char *input) {
    return MODULE_API_CALL(string_create)(input);
}

/**
 * @brief Create an empty string.
 *
 * Constructs an empty managed string object.
 *
 * @returns
 * A managed string object.
 */
MODULE_API string_t *string_construct(void) {
    return MODULE_API_CALL(string_construct)();
}

/**
 * @brief Create a string from format.
 *
 * Constructs a managed string object from a C-style format string and variable
 * arguments.
 *
 * @param input         Format string.
 * @param va            Variable argument list.
 *
 * @returns
 * A managed string object of the formatted string.
 */
MODULE_API string_t *string_vformat(const char *input, va_list va) {
    return MODULE_API_CALL(string_vformat)(input, va);
}

/**
 * @brief Create a string from format.
 *
 * Constructs a managed string object from a C-style format string and variable
 * arguments.
 *
 * @param input         Format string.
 * @param ...           (variable arguments.)
 *
 * @returns
 * A managed string object of the formatted string.
 */
MODULE_API string_t *string_format(const char *input, ...) {
    va_list va;
    va_start(va, input);
    string_t *string = string_vformat(input, va);
    va_end(va);
    return string;
}

/**
 * @brief Concatenate string.
 *
 * Concatenates formatted string to a managed string.
 *
 * @param string        The destination.
 * @param fmt           Format string.
 * @param va            Variable argument list.
 */
MODULE_API void string_vcatf(string_t *string, const char *fmt, va_list va) {
    return MODULE_API_CALL(string_vcatf)(string, fmt, va);
}

/**
 * @brief Concatenate string.
 *
 * Concatenates formatted string to a managed string.
 *
 * @param string        The destination.
 * @param fmt           Format string.
 * @param ...          (variable arguments.)
 */
MODULE_API void string_catf(string_t *string, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    string_vcatf(string, fmt, va);
    va_end(va);
}

/**
 * @brief Shrink string.
 *
 * Shrinks the managed string object by a variable amount of characters. This
 * is done by inserting a null-terminator at length-by.
 *
 * @param string        The string to shrink.
 * @param by            The amount to shrink the string by.
 */
MODULE_API void string_shrink(string_t *string, size_t by) {
    return MODULE_API_CALL(string_shrink)(string, by);
}

/**
 * @brief Query length of string.
 *
 * Query the length of a managed string. Unlike strlen this function is O(1).
 *
 * @param string        The string to get the length of.
 *
 * @returns
 * The length of the managed string.
 */
MODULE_API size_t string_length(string_t *string) {
    return MODULE_API_CALL(string_length)(string);
}

/**
 * @brief Check if string is empty.
 *
 * Checks if the string is empty.
 *
 * @param string        The string to check.
 *
 * @returns
 * True if the string is empty, false otherwise.
 */
MODULE_API bool string_empty(string_t *string) {
    return MODULE_API_CALL(string_empty)(string);
}

/**
 * @brief Get the raw string contents.
 *
 * Obtains a pointer to the raw contents of the managed string.
 *
 * @param string        The string to get the raw contents of.
 *
 * @return
 * A pointer to the raw string buffer.
 */
MODULE_API char *string_contents(string_t *string) {
    return MODULE_API_CALL(string_contents)(string);
}

/**
 * @brief Clear the string
 *
 * Erases the contents of the string and resets the length to 0.
 *
 * @param string        The string to clear.
 */
MODULE_API void string_clear(string_t *string) {
    return MODULE_API_CALL(string_clear)(string);
}

/**
 * @brief Replace occurance in string.
 *
 * Replaces occurance of *substr* with *replace* in a managed string object.
 *
 * @param string        The string to replace in.
 * @param substr        The substring to search for.
 * @param replace       The thing to replace the substring with in *string*.
 *
 * This function only replaces the first occurance in *string*.
 */
MODULE_API void string_replace(string_t *string, const char *substr, const char *replace) {
    return MODULE_API_CALL(string_replace)(string, substr, replace);
}

/** @} */

/** @defgroup Hashtable
 *
 * A hashtable implementation.
 *
 * @{
 */

/**
 * @brief A hashtable.
 *
 * An opaque type for representing a hashtable.
 */
typedef struct hashtable_s hashtable_t;

/**
 * @brief Create a hashtable.
 *
 * @param size          The number of buckets to use for this hashtable.
 *
 * @returns
 * A new hashtable.
 */
MODULE_API hashtable_t *hashtable_create(size_t size) {
    return MODULE_API_CALL(hashtable_create)(size);
}

/**
 * @brief Search the hashtable.
 *
 * Search for an element in the hashtable by key.
 *
 * @param hashtable     The hashtable to search in.
 * @param key           The key.
 *
 * @returns
 * The element if found, NULL otherwise.
 */
MODULE_API void *hashtable_find(hashtable_t *hashtable, const char *key) {
    return MODULE_API_CALL(hashtable_find)(hashtable, key);
}

/**
 * @brief Insert element in hashtable.
 *
 * Insert an element into the hashtable if it doesn't already exist.
 *
 * @param hashtable     The hashtable to insert into.
 * @param key           The key associated with the value.
 * @param value         The value associated with the key.
 */
MODULE_API void hashtable_insert(hashtable_t *hashtable, const char *key, void *value) {
    return MODULE_API_CALL(hashtable_insert)(hashtable, key, value);
}
/** @} */

/** @defgroup IRC
 *
 * The API for communicating, manipulating and querying the IRC instance.
 *
 * @{
 */

/**
 * @brief The IRC handle
 *
 * An opaque type for representing an IRC instance.
 */
typedef struct irc_s irc_t;

/**
 * @brief Write a message on a channel.
 *
 * @param irc           Instance.
 * @param channel       The channel to write the message on.
 * @param fmt           Format string.
 * @param va            Variable argument list.
 */
MODULE_API void irc_writev(irc_t *irc, const char *channel, const char *fmt, va_list va) {
    return MODULE_API_CALL(irc_writev)(irc, channel, fmt, va);
}

/**
 * @brief Write a message on a channel.
 *
 * @param irc           Instance.
 * @param channel       The channel to write the message on.
 * @param fmt           Format string.
 * @param ...           (additional arguments.)
 */
MODULE_API void irc_write(irc_t *irc, const char *channel, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    irc_writev(irc, channel, fmt, va);
    va_end(va);
}

/**
 * @brief Perform an action on a channel.
 *
 * @param irc           Instance.
 * @param channel       The channel to perform the action on.
 * @param fmt           Format string.
 * @param va            Variable argument list.
 */
MODULE_API void irc_actionv(irc_t *irc, const char *channel, const char *fmt, va_list va) {
    return MODULE_API_CALL(irc_actionv)(irc, channel, fmt, va);
}

/**
 * @brief Perform an action on a channel.
 *
 * @param irc           Instance.
 * @param channel       The channel to perform the action on.
 * @param fmt           Format string.
 * @param ...           (additional arguments.)
 */
MODULE_API void irc_action(irc_t *irc, const char *channel, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    irc_actionv(irc, channel, fmt, va);
    va_end(va);
}

/**
 * @brief Join a channel.
 *
 * @param irc           Instance to perform the join on.
 * @param channel       The channel to join.
 */
MODULE_API void irc_join(irc_t *irc, const char *channel) {
    return MODULE_API_CALL(irc_join)(irc, channel);
}

/**
 * @brief Part a channel.
 *
 * @param irc           Instance to perform the part on.
 * @param channel       The channel to part.
 */
MODULE_API void irc_part(irc_t *irc, const char *channel) {
    return MODULE_API_CALL(irc_part)(irc, channel);
}

/**
 * @brief Get a list of loaded modules.
 *
 * @param irc           Instance.
 *
 * @returns
 * A list of `const char *' strings containing the names of modules
 * loaded for the given instance.
 */
MODULE_API list_t *irc_modules_loaded(irc_t *irc) {
    return MODULE_API_CALL(irc_modules_loaded)(irc);
}

/**
 * @brief Get a list of enabled modules for a given channel.
 *
 * @param irc           Instance.
 * @param channel       The channel to check for enabled modules.
 *
 * @returns
 * A list of `const char *' strings containing the names of modules enabled for
 * the given channel.
 */
MODULE_API list_t *irc_modules_enabled(irc_t *irc, const char *channel) {
    return MODULE_API_CALL(irc_modules_enabled)(irc, channel);
}

/**
 * @brief Query module configuration.
 *
 * Module configuration is specified on a per-channel basis in Redroid. The
 * following function queries the module configuration for a given channel.
 *
 * @param irc           Instance.
 * @param channel       The channel to get the module configuration of.
 *
 * @returns
 * A hashtable containing `const char *' values.
 */
MODULE_API hashtable_t *irc_modules_config(irc_t *irc, const char *channel) {
    return MODULE_API_CALL(irc_modules_config)(irc, channel);
}

/**
 * @brief Query channel users.
 *
 * Get a list of users on the current channel.
 *
 * @param irc           Instance.
 * @param channel       The channel to query users from.
 *
 * @returns
 * A list of `const char *' strings containing the name of users for the
 * given channel.
 */
MODULE_API list_t *irc_users(irc_t *irc, const char *channel) {
    return MODULE_API_CALL(irc_users)(irc, channel);
}

/**
 * @brief Query channels.
 *
 * Get a list of channels the current instance is joined to.
 *
 * @param irc           Instance.
 *
 * @returns
 * A list of `const char *' strings containing the names of the channels the
 * instance is currently on.
 */
MODULE_API list_t *irc_channels(irc_t *irc) {
    return MODULE_API_CALL(irc_channels)(irc);
}

/**
 * @brief Query the nickname.
 *
 * Get the nickname of Redroid.
 *
 * @param irc           Instance.
 */
MODULE_API const char *irc_nick(irc_t *irc) {
    return MODULE_API_CALL(irc_nick)(irc);
}

/**
 * @brief Query the channel topic.
 *
 * @param irc           Instance.
 * @param channel       The channel to get the topic from.
 *
 * @returns
 * The topic of the current channel if a topic is set, "(No topic)" if there is
 * no topic set for the channel and *NULL* if not on the channel.
 */
MODULE_API const char *irc_topic(irc_t *irc, const char *channel) {
    return MODULE_API_CALL(irc_topic)(irc, channel);
}

/**
 * @brief Query the pattern.
 *
 * The pattern is the character or string which all commands must be prefixed with
 * for the bot to recgonize it as a command. Supplying NULL for *newpattern* will
 * return the current pattern, for anything else the pattern will be replaced.
 *
 * @param irc           Instance.
 * @param newpattern    A new pattern to replace the old pattern with.
 *
 * @returns
 * The currently set pattern.
 */
MODULE_API const char *irc_pattern(irc_t *irc, const char *newpattern) {
    return MODULE_API_CALL(irc_pattern)(irc, newpattern);
}

/**
 * @brief Module operation status codes.
 *
 * The following status codes may be returned by functions which manipulate
 * module state.
 */
typedef enum {
    MODULE_STATUS_REFERENCED, /**< Operation couldn't complete because the module is referenced elsewhere. */
    MODULE_STATUS_SUCCESS,    /**< Operation success. */
    MODULE_STATUS_FAILURE,    /**< Operation failure. */
    MODULE_STATUS_ALREADY,    /**< Operation is redundant, already in correct state. */
    MODULE_STATUS_NONEXIST    /**< Operation on non-existant module. */
} module_status_t;

/** @brief Load a module.
 *
 * Loads a module into the instance.
 *
 * @param irc           Instance.
 * @param file          The module to load *as filename.*
 *
 * @returns
 * One of the status codes of #module_status_t.
 */
MODULE_API module_status_t irc_modules_add(irc_t *irc, const char *file) {
    return MODULE_API_CALL(irc_modules_add)(irc, file);
}

/** @brief Reload a module.
 *
 * Reloads a module for the instance.
 *
 * @param irc           Instance.
 * @param name          The module to reload *as module name.*
 *
 * @returns
 * One of the status codes of #module_status_t.
 */
MODULE_API module_status_t irc_modules_reload(irc_t *irc, const char *name) {
    return MODULE_API_CALL(irc_modules_reload)(irc, name);
}

/**
 * @brief Unload a module.
 *
 * Unloading a module will fail if the module is referenced on (another channel /
 * other channels.) You can force to unload the module with *force* which will
 * ignore this constraint. The name of the current channel that invoked the unload
 * must be supplied as well since it's excluded in the reference count to determine
 * if a module is referenced elsewhere.
 *
 * @param irc           Instance.
 * @param channel       The current channel which requested the unload.
 * @param name          The module name.
 * @param force         Force unloading even if the module is referenced in another
 *                      channel.
 *
 * @returns
 * One of the status codes of #module_status_t.
 */
MODULE_API module_status_t irc_modules_unload(irc_t *irc, const char *channel, const char *name, bool force) {
    return MODULE_API_CALL(irc_modules_unload)(irc, channel, name, force);
}

/**
 * @brief Disable a module.
 *
 * Disable a module on a channel.
 *
 * @param irc           Instance.
 * @param channel       The channel to disable the module on.
 * @param name          The name of the module to disable.
 *
 * @returns
 * One of the status codes of #module_status_t.
 */
MODULE_API module_status_t irc_modules_disable(irc_t *irc, const char *channel, const char *name) {
    return MODULE_API_CALL(irc_modules_disable)(irc, channel, name);
}

/**
 * @brief Enable a module.
 *
 * Enables a module on a channel.
 *
 * @param irc           Instance.
 * @param channel       The channel to enable the module on.
 * @param name          The name of the module to enable.
 *
 * @returns
 * One of the status codes of #module_status_t.
 */
MODULE_API module_status_t irc_modules_enable(irc_t *irc, const char *channel, const char *name) {
    return MODULE_API_CALL(irc_modules_enable)(irc, channel, name);
}

/** @} */

/** @defgroup Database
 *
 * Database manipulation.
 *
 * @{
 */

/**
 * @brief A database statement.
 *
 * An opaque type for representing a database statement.
 */
typedef struct database_statement_s database_statement_t;

/**
 * @brief A database row.
 *
 * An opaque type for representing a row in a database..
 */
typedef struct database_row_s database_row_t;

/**
 * @brief Create a database statement.
 *
 * Create a database statement from SQL-like statement with `?' characters as
 * binding points.
 *
 * @param string        The SQL-like statement as a string.
 *
 * @returns
 * A database statement if the statement specified by *string* is valid, NULL
 * otherwise.
 */
MODULE_API database_statement_t *database_statement_create(const char *string) {
    return MODULE_API_CALL(database_statement_create)(string);
}

/**
 * @brief Bind variables to a statement.
 *
 * Binds variables to a statement by using a mapping string which specifies the
 * variable types as well as additional variable arguments of the variables to
 * bind to the statement following that mapping.
 *
 * Variable mapping specification allows for specifying the types of the variable
 * arguments in much the same way printf format specifiers work. The mapping must
 * be in the same order as the statement bind points. The mapping string can be
 * composed of any of the following characters
 *      -   'i' *int*
 *      -   's' *const char* *
 *      -   'S' *string_t* *
 *
 * Example:
 * @code
 * database_statement_t *foo = database_statement_create("SELECT ? FROM ? WHERE A = ?");
 * database_statement_bind(foo, "ssi", "hi", "bye", 100);
 * // a.k.a SELECT hi FROM bye WHERE A=100
 * @endcode
 *
 * @param statement     The statement to bind variables to.
 * @param mapping       The variable mapping specification.
 * @param va            Variable argument list.
 *
 * @returns
 * If the statement and mapping is valid and the binding was successful true is
 * returned, otherwise false.
 */
MODULE_API bool database_statement_bindv(database_statement_t *statement, const char *mapping, va_list va) {
    return MODULE_API_CALL(database_statement_bindv)(statement, mapping, va);
}

/**
 * @brief Bind variables to a statement.
 *
 * See #database_statement_bindv for information.
 *
 * @param statement     The statement to bind variables to.
 * @param mapping       The variable mapping specification.
 * @param va            Variable argument list.
 *
 * @returns
 * If the statement and mapping is valid and the binding was successful true is
 * returned, otherwise false.
 */
MODULE_API bool database_statement_bind(database_statement_t *statement, const char *mapping, ...) {
    va_list va;
    va_start(va, mapping);
    bool result = database_statement_bindv(statement, mapping, va);
    va_end(va);
    return result;
}

/**
 * @brief Query if statement is complete
 *
 * Check to see if a statement completed.
 *
 * @param statement     The statement to check for completion.
 *
 * @returns
 * If the statement completed successfully true is returned, otherwise false.
 */
MODULE_API bool database_statement_complete(database_statement_t *statement) {
    return MODULE_API_CALL(database_statement_complete)(statement);
}

/**
 * @brief Extract a row from the database.
 *
 * Given a statement, extract the current row.
 *
 * @param statement     The statement to extract row from.
 * @param fields        Mapping specification.
 *
 * @returns
 * If the statement and mapping specification is valid, a row, otherwise false.
 */
MODULE_API database_row_t *database_row_extract(database_statement_t *statement, const char *fields) {
    return MODULE_API_CALL(database_row_extract)(statement, fields);
}

/**
 * @brief Pop a string off the row.
 *
 * @param row           The row to extract a string from.
 *
 * @returns
 * If the thing to extract is a string, a string is returned, otherwise false.
 */
MODULE_API const char *database_row_pop_string(database_row_t *row) {
    return MODULE_API_CALL(database_row_pop_string)(row);
}

/**
 * @brief Pop an integer off the row.
 *
 * @param row           The row to extract an integer from.
 *
 * @returns
 * If the thing to extract is an integer, an integer is returned, otherwise
 * -1 is returned.
 */
MODULE_API int database_row_pop_integer(database_row_t *row) {
    return MODULE_API_CALL(database_row_pop_integer)(row);
}

/**
 * @brief Increment the request count for a table.
 *
 * @param instance      The IRC instance which requested the table.
 * @param table         The table requested by that instance.
 *
 * @returns
 * If the request count for the table could be incremented, true is returned,
 * otherwise false.
 */
MODULE_API bool database_request(irc_t *instance, const char *table) {
    return MODULE_API_CALL(database_request)(instance, table);
}

/**
 * @brief Get the request count for a table.
 *
 * @param instance      The IRC instance requesting the count of a given table.
 * @param table         The table to get the request count of.
 *
 * @returns
 * The amount of time that table was requested.
 */
MODULE_API int database_request_count(irc_t *instance, const char *table) {
    return MODULE_API_CALL(database_request_count)(instance, table);
}

/** @} */

/** @defgroup Regex
 *
 * Regular expression matching.
 *
 * @{
 */

/**
 * @brief A regular expression.
 *
 * An opaque type for representing a regular expression.
 */
typedef struct regexpr_s regexpr_t;

/**
 * @brief Type used to represent a match.
 *
 * A match is represented as starting and ending offsets (*soff* and *eoff*) of
 * the string to search in. This effectively gives you a substring which contains
 * the match. Invalid matches are indicated when both *soff* and *eoff* are -1.
 */
typedef struct {
    /** The offset to the starting of the string where a match was found. */
    int soff;

    /** The offset to the ending of the string where a match was found. */
    int eoff;
} regexpr_match_t;

#define regexpr_match_invalid(X) \
    ((X).soff == -1 || ((X).eoff == -1))

/**
 * @brief Create a regular expression.
 *
 * @param string                Regular expression *as a string*.
 * @param icase                 Ignore case.
 *
 * @returns
 * A regular expression object.
 */
MODULE_API regexpr_t *regexpr_create(const char *string, bool icase) {
    return MODULE_API_CALL(regexpr_create)(string, icase);
}

/**
 * @brief Execute a regular expression.
 *
 * @param expr                  The regular expression.
 * @param string                The string to run the regular expression over.
 * @param nmatch                Maximum number of matches.
 * @param array                 Handle to store an array of pointers.
 *
 * @returns
 * If any match is found true is returned, otherwise false.
 */
MODULE_API bool regexpr_execute(const regexpr_t *expr, const char *string, size_t nmatch, regexpr_match_t **array) {
    return MODULE_API_CALL(regexpr_execute)(expr, string, nmatch, array);
}
/** @} */

/** @defgroup Random
 *
 * Random number generation faccilities.
 *
 * @{
 */

/**
 * @brief Generate a random unsigned integer.
 *
 * @returns
 * A random unsigned integer.
 */
MODULE_API unsigned int urand(void) {
    return MODULE_API_CALL(urand)();
}

/**
 * @brief Generate a random dobule.
 *
 * @returns
 * A random double.
 */
MODULE_API double drand(void) {
    return MODULE_API_CALL(drand)();
}
/** @} */

/** @defgroup Redroid
 *
 * Bot manipulation.
 *
 * @{
 */

/**
 * @brief Restart Redroid.
 *
 * Will attempt to restart Redroid without disconnecting from any IRC instances.
 *
 * @param irc               The instance the restart originated from.
 * @param channel           The channel the restart originated from.
 * @param user              The user the restarted originated from.
 */
MODULE_API void redroid_restart(irc_t *irc, const char *channel, const char *user) {
    return MODULE_API_CALL(redroid_restart)(irc, channel, user);
}

/**
 * @brief Shutdown Redroid.
 *
 * Will attempt to shutdown Redroid.
 *
 * @param irc               The instance the shutdown originated from.
 * @param channel           The channel the shutdown originated from.
 * @param user              The user the shutdown originated from.
 */
MODULE_API void redroid_shutdown(irc_t *irc, const char *channel, const char *user) {
    return MODULE_API_CALL(redroid_shutdown)(irc, channel, user);
}

/**
 * @brief Recompile and restart Redroid.
 *
 * Will attempt to recompile Redroid from source and then restart into the
 * new binary without disconnecting from any IRC instances.
 *
 * @param irc               The instance the recompile originated from.
 * @param channel           The channel the recompile originated from.
 * @param user              The user the recompile originated from.
 */
MODULE_API void redroid_recompile(irc_t *irc, const char *channel, const char *user) {
    return MODULE_API_CALL(redroid_recompile)(irc, channel, user);
}

/**
 * @brief Daemonize Redroid.
 *
 * Will attempt to datmonize Redroid. Daemonization is the process of putting
 * Redroid in the background as a service. For instance launching Redroid in a
 * terminal will make you lose the terminal, or launching Redroid from a ssh
 * session and closing the session will kill Redroid. A call to this will detach
 * it from the terminal such that the process become a daemon.
 *
 * @param irc               The instance the daemonization originated from.
 * @param channel           The channel the daemonization originated from.
 * @param user              The user the daemonization originated from.
 */
MODULE_API void redroid_daemonize(irc_t *irc, const char *channel, const char *user) {
    return MODULE_API_CALL(redroid_daemonize)(irc, channel, user);
}

/**
 * @brief Get build information of Redroid.
 *
 * Obtain the build time and date stamp of Redroid as a formatted string.
 *
 * @returns
 * Build information of Redroid.
 */
MODULE_API const char *redroid_buildinfo(void) {
    return MODULE_API_CALL(redroid_buildinfo)();
}

/** @} */


/** @defgroup Access
 *  Administrative API.
 * @{
 */

typedef enum {
    ACCESS_NOEXIST_TARGET, /**< Target doesn't exist in access database. */
    ACCESS_NOEXIST_INVOKE, /**< Invoker of access doesn't exist in access database. */
    ACCESS_EXISTS,         /**< Already exists. */
    ACCESS_DENIED,         /**< Denied access. */
    ACCESS_SUCCESS,        /**< Access allowed. */
    ACCESS_FAILED,         /**< Access check failed. */
    ACCESS_BADRANGE        /**< Invalid access range. */
} access_t;


/**
 * @brief Check if target is in range.
 *
 * @param irc               The instance.
 * @param target            The target to check.
 * @param check             The access level to check if in range of.
 *
 * @returns
 * If the target is in range, returns true, false otherwise.
 */
MODULE_API bool access_range(irc_t *irc, const char *target, int check) {
    return MODULE_API_CALL(access_range)(irc, target, check);
}


/**
 * @brief Check if target is of access level.
 *
 * @param irc               The instance.
 * @param target            The target to check.
 * @param check             The access level to check.
 *
 * @returns
 * If the target is of that access level, returns true, false otherwise.
 */
MODULE_API bool access_check(irc_t *irc, const char *target, int check) {
    return MODULE_API_CALL(access_check)(irc, target, check);
}

/**
 * @brief Get access level of target.
 *
 * @param irc               The instance.
 * @param target            The target to get access level of.
 * @param level             Pointer to integer to store access level to.
 *
 * @returns
 * If the target exists in the access database, returns true, false otherwise.
 */
MODULE_API bool access_level(irc_t *irc, const char *target, int *level) {
    return MODULE_API_CALL(access_level)(irc, target, level);
}

/**
 * @brief Remove access for target.
 *
 * @param irc               The instance.
 * @param target            The target.
 * @param invoke            The invoker.
 *
 * @returns
 * One of the status codes in #access_t.
 */
MODULE_API access_t access_remove(irc_t *irc, const char *target, const char *invoke) {
    return MODULE_API_CALL(access_remove)(irc, target, invoke);
}

/**
 * @brief Insert access for a new target.
 *
 * @param irc               The instance.
 * @param target            The target.
 * @param invoke            The invoker.
 * @param level             The level to give the target.
 *
 * @returns
 * One of the status codes in #access_t.
 */
MODULE_API access_t access_insert(irc_t *irc, const char *target, const char *invoke, int level) {
    return MODULE_API_CALL(access_insert)(irc, target, invoke, level);
}

/**
 * @brief Change access for a target.
 *
 * @param irc               The instance.
 * @param target            The target.
 * @param invoke            The invoker.
 * @param level             The level to give the target.
 *
 * @returns
 * One of the status codes in #access_t.
 */
MODULE_API access_t access_change(irc_t *irc, const char *target, const char *invoke, int level) {
    return MODULE_API_CALL(access_change)(irc, target, invoke, level);
}

/** @} */

/** @defgroup Miscellaneous
 *
 * Miscellaneous uncategorizable facilities.
 *
 * @{
 */
typedef struct {
    /** The messsage. */
    string_t *message;
    /** The author. */
    string_t *author;
    /** The revision. */
    string_t *revision;
} svn_entry_t;

/**
 * @brief Split a string by delimiter into a list.
 *
 * @param string            The string to split.
 * @param delimiter         The delimiter to split the string by.
 *
 * @returns
 * A list of `const char *' strings split by that delimiter.
 */
MODULE_API list_t *strsplit(const char *string, const char *delimiter) {
    return MODULE_API_CALL(strsplit)(string, delimiter);
}

/**
 * @brief Split a string by delimiter into a list with a limit.
 *
 * @param string            The string to split.
 * @param delimiter         The delimiter to split the string by.
 * @param count             An upper limit for splits.
 *
 * @returns
 * A list of `const char *' strings split by that delimiter. If the upper limit
 * was met the last element in the list may contain strings still containing the
 * delimiter.
 */
MODULE_API list_t *strnsplit(const char *string, const char *delimiter, size_t count) {
    return MODULE_API_CALL(strnsplit)(string, delimiter, count);
}

/**
 * @brief Get SVN log.
 *
 * @param url               The URL or local URI to an SVN repository.
 * @param depth             The depth of commits to read.
 *
 * @returns
 * A list of #svn_entry_t *s containing SVN commit messages.
 */
MODULE_API list_t *svnlog(const char *url, size_t depth) {
    return MODULE_API_CALL(svnlog)(url, depth);
}

/**
 * @brief Duration as a string.
 *
 * Given seconds of duration construct a string like "4m3w2d1s".
 *
 * @param dur               Seconds of duration.
 *
 * @returns
 * Duration as a string.
 */
MODULE_API const char *strdur(unsigned long long dur) {
    return MODULE_API_CALL(strdur)(dur);
}

/**
 * @brief DNS Resolve
 *
 * Resolve a domain name.
 *
 * @param url               The URL
 *
 * @returns
 * A list of `const char *' strings containing the resolved ip-addresses.
 */
MODULE_API list_t *dns(const char *url) {
    return MODULE_API_CALL(dns)(url);
}

/** @} */

#endif
