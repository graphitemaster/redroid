#include "http.h"
#include "mt.h"
#include "ripemd.h"
#include "database.h"
#include "string.h"

#include <string.h>
#include <stdio.h>

typedef struct {
    mt_t       *rand;
    ripemd_t   *ripemd;
    database_t *database;
} web_t;

void web_login(sock_t *client, list_t *post, void *data) {
    web_t *web = data;

    const char *username = http_post_find(post, "username");
    const char *password = http_post_find(post, "password");

    database_statement_t *statement = database_statement_create(web->database, "SELECT SALT FROM USERS WHERE USERNAME=?");
    database_row_t       *row;

    if (!statement)
        return http_send_redirect(client, "site/invalid.html");
    if (!database_statement_bind(statement, "s", username))
        return http_send_redirect(client, "site/invalid.html");
    if (!(row = database_row_extract(statement, "s")))
        return http_send_redirect(client, "site/invalid.html");

    const char    *getsalt         = database_row_pop_string(row);
    string_t      *saltpassword    = string_format("%s%s", getsalt, password);
    const char    *saltpasswordraw = string_contents(saltpassword);
    unsigned char *ripemdpass      = ripemd_compute(web->ripemd, saltpasswordraw, string_length(saltpassword));
    string_t      *hashpassword    = string_construct();

    string_destroy(saltpassword);
    database_row_destroy(row);

    for (size_t i = 0; i < 160 / 8; i++)
        string_catf(hashpassword, "%02x", ripemdpass[i]);

    statement = database_statement_create(web->database, "SELECT COUNT(*) FROM USERS WHERE USERNAME=? AND PASSWORD=?");
    if (!statement)
        return string_destroy(hashpassword);
    if (!database_statement_bind(statement, "sS", username, hashpassword))
        return string_destroy(hashpassword);
    if (!(row = database_row_extract(statement, "i")))
        return string_destroy(hashpassword);

    string_destroy(hashpassword);

    if (database_row_pop_integer(row) != 0)
        http_send_plain(client, "valid username and password");
    else
        http_send_plain(client, "invalid username or password");

    database_row_destroy(row);
}

#if TEST
int main() {
    http_t *server = http_create("5050");

    web_t web = {
        .rand     = mt_create(),
        .ripemd   = ripemd_create(),
        .database = database_create("web.db")
    };

    http_handles_register(server, "::login", &web_login, &web);

    while (1)
        http_process(server);

    http_destroy(server);
    mt_destroy(web.rand);
    ripemd_destroy(web.ripemd);
}
#endif
