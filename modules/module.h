#ifndef REDROID_MODULE_MODULE_HDR
#define REDROID_MODULE_MODULE_HDR

#include <stdbool.h>
#include <stddef.h>

struct addrinfo;
struct database_statement_s;
struct database_row_s;
struct irc_s;
struct list_s;
struct list_iterator_s;
struct string_s;

typedef struct database_statement_s     database_statement_t;
typedef struct database_row_s           database_row_t;
typedef struct irc_s                    irc_t;
typedef struct list_s                   list_t;
typedef struct list_iterator_s          list_iterator_t;
typedef struct string_s                 string_t;

#define MODULE_DEFAULT(NAME) char module_name[]  = #NAME, module_match[] = #NAME
#define MODULE_ALWAYS(NAME)  char module_name[]  = #NAME, module_match[] = ""

/*
 * Macro: MODULE_GC_CALL
 *  Call a garbage collected function.
 *
 * Parameters:
 *  NAME    - Name of the function to call
 *
 * Remarks:
 *  This should be avoided as the functions you intend to call are
 *  already provided by this same header and are already marked to call
 *  the garbage collector if they allocate or return allocated resources.
 */
#define MODULE_GC_CALL(NAME)               \
    extern __typeof__(NAME) module_##NAME; \
    return module_##NAME

/*
 * Function: irc_modules_list
 *  Obtain a list of the current loaded modules for the IRC instance.
 *
 * Returns:
 *  A list of character pointers to strings containing the names of
 *  the loaded modules for a given IRC instance.
 *
 * Parameters:
 *  irc - IRC instance
 *
 * Remarks:
 *  The list this function returns is garbage collected.
 */
static inline list_t *irc_modules_list(irc_t *irc) {
    MODULE_GC_CALL(irc_modules_list)(irc);
}

/*
 * Function: malloc
 *  Allocate a block of memory.
 *
 * Parameters:
 *  size    - The size in bytes to allocate
 *
 * Returns:
 *  Pointer to the beginning of the allocated block of memory *size* in
 *  bytes.
 *
 * Remarks:
 *  The content of the newly allocated block of memory is default
 *  initialized to 0. This is to ensure modules which allocate memory
 *  are fully comitted.
 *
 *  The memory this function returns a pointer to is garbage collected.
 */
static inline void *malloc(size_t size) {
    MODULE_GC_CALL(malloc)(size);
}

/*
 * Function: string_create
 *  Create a managed string object with a default value.
 *
 * Returns:
 *  Pointer to a managed string object default initialied with
 *  *input*
 *
 * Parameters:
 *  input   - The input to default initialize the managed string with
 *
 * Remarks:
 *  The managed string this function returns is garbage collected.
 */
static inline string_t *string_create(const char *input) {
    MODULE_GC_CALL(string_create)(input);
}

/*
 * Function: string_construct
 *  Create a managed string object.
 *
 * Returns:
 *  Pointer to a managed string object with no contents.
 *
 * Remarks:
 *  The managed string this function returns is garbage collected.
 */
static inline string_t *string_construct(void) {
    MODULE_GC_CALL(string_construct)();
}

/*
 * Function: list_iterator_create
 *  Create an iterator for a list object.
 *
 * Returns:
 *  An iterator for the list object.
 *
 * Remarks:
 *  The iterator this function returns is garbage collected.
 */
static inline list_iterator_t *list_iterator_create(list_t *list) {
    MODULE_GC_CALL(list_iterator_create)(list);
}

/*
 * Function: list_create
 *  Create a list
 *
 * Returns:
 *  A new list
 *
 * Remarks:
 *  The list this function returns is garbage collected.
 */
static inline list_t *list_create(void) {
    MODULE_GC_CALL(list_create)();
}

/*
 * Function: list_push
 *  Append an element to the list.
 *
 * Parameters:
 *  list    - Pointer to the list object
 *  element - Pointer to the element to append to the list
 *
 * Remarks:
 *  The resources this function allocates are garbage collected.
 */
static inline void list_push(list_t *list, void *element) {
    MODULE_GC_CALL(list_push)(list, element);
}

/*
 * Function: getaddrinfo
 *  Network address and service translation.
 *
 * Parameters:
 *  mode    - Internet host
 *  service - Internet service
 *  hints   - Critera for selecting the socket address structures returned in the list pointed by *result*
 *  result  - Allocated linked list containing result
 *
 * Returns:
 *  0 if it succeeds.
 *
 * Remarks:
 *  For more information see the offical documentation on getaddrinfo.
 *
 *  The resources allocated by this function for the resulting linked list
 *  are garbage collected.
 */
static inline int getaddrinfo(const char *mode, const char *service, const struct addrinfo *hints, struct addrinfo **result) {
    MODULE_GC_CALL(getaddrinfo)(mode, service, hints, result);
}

/*
 * Function: database_statement_create
 *  Create a prepared database statement
 *
 * Parameters:
 *  string - A valid SDL prepared statement
 *
 * Returns:
 *  On success: pointer to a database statement
 *  On failure: NULL pointer;
 *
 * Remarks:
 *  The statement this function returns is garbage collected.
 */
static inline database_statement_t *database_statement_create(const char *string) {
    MODULE_GC_CALL(database_statement_create)(string);
}

/*
 * Function: database_row_extract
 *  Extract the row contents of a prepared statement
 *
 * Parameters:
 *  statement   - Pointer to the prepared statement
 *  fields      - A type specification in the form of a string to extract
 *
 * Returns:
 *  A variant-typed linked-list row structure containing elements typed
 *  in the order their specification was provided by *fields*, on success.
 *  On failure NULL is returned.
 *
 * Remarks:
 *  A type specification is a string containing the types intended to be
 *  specified in the database as individual characters. Currently two
 *  types are legal for database API, string and int. These two types
 *  are described as 's' and 'i'. Thus when extracting a row containing
 *  string, int, int, string; *fields* should be "siis".
 *
 *  The resources allocated and returned by this function are garbage
 *  collected.
 */
static inline database_row_t *database_row_extract(database_statement_t *statement, const char *fields) {
    MODULE_GC_CALL(database_row_extract)(statement, fields);
}
/*
 * Function: database_row_pop_string
 *  Pop a string from a database row.
 *
 * Parameters:
 *  row - Pointer to the row to pop the string from
 *
 * Returns:
 *  A string containing the contents poped from the row.
 *
 * Remarks:
 *  This function should only be called when the next thing to pop
 *  from a row is really a string, otherwise it may produce strange
 *  results as the variant type of the rows internal link-list node
 *  is that of a union.
 *
 *  The allocated resources returned by this function are garbage
 *  collected.
 */
static inline const char *database_row_pop_string(database_row_t *row) {
    MODULE_GC_CALL(database_row_pop_string)(row);
}

/*
 * Function: strdup
 *  Copy a string
 *
 * Parameters:
 *  string  - The string to copy
 *
 * Returns:
 *  A copied string
 *
 * Remarks:
 *  The string returned by this function is garbage collected.
 */
static inline char *strdup(const char *string) {
    MODULE_GC_CALL(strdup)(string);
}

static inline list_t *strsplit(char *string, char *delimiter) {
    MODULE_GC_CALL(strsplit)(string, delimiter);
}
static inline list_t *strnsplit(char *string, char *delimiter, size_t count) {
    MODULE_GC_CALL(strnsplit)(string, delimiter, count);
}

/*
 * Function: list_iterator_reset
 *  Reset the position of a list iterator
 *
 * Parmeters:
 *  iterator    - The iterator to reset
 */
void list_iterator_reset(list_iterator_t *iterator);

/*
 * Function: list_iterator_end
 *  Check if the iterator is at the end of the list
 *
 * Parmeters:
 *  iterator    - The iterator to test
 *
 * Returns:
 *  true if at the end of the list, false otherwise.
 */
bool list_iterator_end(list_iterator_t *iterator);

/*
 * Function: list_iterator_next
 *  Get the element at the iterator position
 *
 * Parameters:
 *  iterator    - The iterator
 *
 * Returns:
 *  The element at that position
 *
 * Remarks:
 *  This advances the iterators position by one.
 */
void *list_iterator_next(list_iterator_t *iterator);

/*
 * Function: list_pop
 *  Pop the element off the head of the list
 *
 * Parameters:
 *  list    - The list to pop element off of
 *
 * Returns:
 *  The element at the head of the list.
 */
void *list_pop(list_t *list);

/*
 * Function: list_length
 *  Get the length of the list
 *
 * Parameters:
 *  list    - The list to get the length of
 *
 * Returns:
 *  The number of elements in the list.
 */
size_t list_length(list_t *list);

/*
 * Function: string_catf
 *  Concatenate a formatted string to a managed string
 *
 * Parameters:
 *  string  - The managed string to concatenate to
 *  fmt     - Format specification for the string to concatenate
 *  ...     - Variable arguments for the format specification string
 */
void string_catf(string_t *string, const char *fmt, ...);


/*
 * Function: string_length
 *  Get the length of a managed string
 *
 * Parmaters:
 *  string  - The managed string
 *
 * Returns:
 *  The length of the string, or 0 if the string passed points to NULL.
 *
 * Remarks:
 *  Managed string keeps its length around internally to allow for
 *  dynamically resizing operations like string concatenation. This lends
 *  itself to making the complexity of string length calculation entierly
 *  constant. Thus this function has O(1) running time.
 */
size_t string_length(string_t *string);

/*
 * Function: string_empty
 *  Check if a string is empty
 *
 * Parameters:
 *  string  - The managed string
 *
 * Returns:
 *  true if the contents of the string are nothing, or if the string
 *  passed points to NULL, false otherwise.
 */
bool string_empty(string_t *string);

/*
 * Function: string_contents
 *  Get the raw contents of a managed string
 *
 * Paramaters:
 *  string - The managed string
 *
 * Returns:
 *  Pointer to the raw contents of the string (internal buffer).
 */
char *const string_contents(string_t *string);

bool database_statement_complete(database_statement_t *statement);
bool database_statement_bind(database_statement_t *statement, const char *mapping, ...);
database_row_t *database_row_extract(database_statement_t *statement, const char *fields);
const char *database_row_pop_string(database_row_t *row);
int  database_row_pop_integer(database_row_t *row);
bool database_request(irc_t *instance, const char *table);
int database_request_count(irc_t *instance, const char *table);
bool irc_modules_reload(irc_t *irc, const char *name);
bool irc_modules_add(irc_t *irc, const char *file);
int irc_write(irc_t *irc, const char *channel, const char *fmt, ...);
int irc_action(irc_t *irc, const char *channel, const char *fmt, ...);
const char *irc_nick(irc_t *irc);
list_t *irc_modules(irc_t *irc);

#endif
