#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

int main(void) {
    FILE *fp = fopen("misc/whitelist", "r");
    FILE *db = fopen("/tmp/redroid_db_gen", "w");

    if (!fp || !db)
        return EXIT_FAILURE;

    printf("Generating entries ...");

    char   *line  = NULL;
    size_t  size  = 0;
    size_t  count = 0;
    bool    libc  = true;

    while (getline(&line, &size, fp) != EOF) {
        // lines beginning with '#' are comments
        if (*line == '#')
            continue;

        // empty line denotes beginning of non libc section
        if (*line == '\n') {
            libc = false;
            continue;
        }

        // strip newline
        *strchr(line, '\n')='\0';
        fprintf(db, "INSERT INTO WHITELIST VALUES('%s', %d);\n", line, libc);
        count++;
    }

    free(line);
    fclose(fp);
    fclose(db);

    printf("\tSUCCESS (%d entries created)\n", count);

    printf("Creating database ...");
    if (system("echo \"CREATE TABLE IF NOT EXISTS WHITELIST (NAME TEXT, LIBC INTEGER);\" | sqlite3 whitelist.db") != EXIT_SUCCESS) {
        printf("\tERROR\n");
        remove("/tmp/redroid_db_gen");
        return EXIT_FAILURE;
    }
    printf("\tSUCCESS\nPopulating database ...");

    if (system("cat /tmp/redroid_db_gen | sqlite3 whitelist.db") != EXIT_SUCCESS) {
        printf("\tERROR\n");
        remove("/tmp/redroid_db_gen");
        return EXIT_FAILURE;
    }

    remove("/tmp/redroid_db_gen");
    printf("\tSUCCESS\n");

    return EXIT_SUCCESS;
}
