#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sqlite3.h>

int main(int argc, char **argv) {
    argv++;
    argc--;

    bool debug   = argc && !strcmp(*argv, "-d");
    int  exitval = EXIT_SUCCESS;

    sqlite3 *database;
    if (sqlite3_open("whitelist.db", &database) != SQLITE_OK)
        return EXIT_FAILURE;

    char *errmsg = NULL;
    if (sqlite3_exec(database, "CREATE TABLE IF NOT EXISTS WHITELIST (NAME TEXT, LIBC INTEGER)", NULL, NULL, &errmsg) != SQLITE_OK) {
        fprintf(stderr, "internal error: %s\n", errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(database);
        return EXIT_FAILURE;
    }

    sqlite3_stmt *insert = NULL;
    if (sqlite3_prepare_v2(database, "INSERT INTO WHITELIST VALUES(?, ?)", -1, &insert, NULL) != SQLITE_OK)
        goto prep_error;

    sqlite3_stmt *find = NULL;
    if (sqlite3_prepare_v2(database, "SELECT LIBC FROM WHITELIST WHERE NAME = ?", -1, &find, NULL) != SQLITE_OK) {
    prep_error:
        fprintf(stderr, "failed to prepare statement\n");
        if (insert) sqlite3_finalize(insert);
        if (find)   sqlite3_finalize(find);

        sqlite3_close(database);
        return EXIT_FAILURE;
    }

    // read the whitelist and fill the database
    char  *line   = NULL;
    bool   libc   = false;
    size_t size   = 0;
    size_t count  = 0;
    size_t exist  = 0;
    size_t lineno = 0;
    FILE  *fp     = fopen("misc/whitelist", "r");

    if (!fp)
        goto process;

    char *err = NULL;
    sqlite3_exec(database, "BEGIN TRANSACTION", NULL, NULL, &err);
    sqlite3_free(err);

    while (getline(&line, &size, fp) != EOF) {
        lineno++;

        if (*line == '#')
            continue;
        if (*line == '\n') {
            libc = false;
            continue;
        }

        *strchr(line, '\n')='\0';

        sqlite3_reset(find);
        size_t error;
        if ((error = sqlite3_bind_text(find, 1, line, strlen(line), NULL)) != SQLITE_OK) {
            fprintf(stderr, "failed to bind name `%s` %zu\n", line, error);
            goto internal_error_silent;
        }

        if (sqlite3_step(find) == SQLITE_ROW) {
            if (debug)
                printf("duplicate: %zu => %s\n", lineno, line);
            exist++;
            continue;
        }

        count++;

        sqlite3_reset(insert);
        if (sqlite3_bind_text(insert, 1, line, strlen(line), NULL) != SQLITE_OK)
            goto internal_error;
        if (sqlite3_bind_int(insert, 2, libc) != SQLITE_OK)
            goto internal_error;
        if (sqlite3_step(insert) != SQLITE_DONE)
            goto internal_error;
    }

    sqlite3_exec(database, "COMMIT TRANSACTION", NULL, NULL, &err);
    sqlite3_free(err);

    goto process;

internal_error:
    fprintf(stderr, "internal error\n");
internal_error_silent:
    exitval = EXIT_FAILURE;
process:
    if (!fp) {
        fprintf(stderr, "failed to open whitelist for processing\n");
        exitval = EXIT_FAILURE;
    }

    if (line)
        free(line);
    if (fp)
        fclose(fp);

    sqlite3_finalize(insert);
    sqlite3_finalize(find);

    if (debug) {
        printf("Entries:\n");
        printf("    added:    %zu\n", count);
        printf("    existing: %zu\n", exist);
    }

    sqlite3_close(database);

    return exitval;
}
