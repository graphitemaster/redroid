#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>

void table(const char *file, const char *table, size_t fields) {
    FILE *fp = fopen(file, "r");
    if (!fp)
        return;

    printf("create table if not exists %s (NAME, CONTENT);\n", table);
    printf("INSERT INTO REQUESTS (NAME, COUNT) VALUES ('%s', 0);\n", table);

    size_t size = 0;
    char  *line = 0;

    while (getline(&line, &size, fp) != EOF) {
        char *name = strdup(line);
        char *content = strchr(name, ' ') + 1;
        *strchr(name, ' ') = '\0';
        char *nl = strchr(content, '\n');
        if (nl) *nl = '\0';
        char *esc = sqlite3_mprintf("INSERT INTO %s (NAME, CONTENT) VALUES ('%q', '%q');", table, name, content);
        printf("%s\n", esc);
        sqlite3_free(esc);
        free(name);
    }
    free(line);
    fclose(fp);
}

void faq(const char *file) {
    FILE *fp = fopen(file, "r");
    if (!fp)
        return;

    printf("create table if not exists FAQ (NAME, AUTHOR, CONTENT);\n");
    printf("INSERT INTO REQUESTS (NAME, COUNT) VALUES ('FAQ', 0);\n");

    size_t size = 0;
    char  *line = 0;

    while (getline(&line, &size, fp) != EOF) {
        char *name    = strdup(line);
        char *author  = strdup(strchr(name, ' ') + 1);
        char *content = strchr(author, ' ') + 1;

        *strchr(name,   ' ') = '\0';
        *strchr(author, ' ') = '\0';

        char *nl = strchr(content, '\n');
        if (nl) *nl = '\0';
        char *esc = sqlite3_mprintf("INSERT INTO FAQ (NAME, AUTHOR, CONTENT) VALUES ('%q', '%q', '%q');", name, author, content);
        printf("%s\n", esc);
        sqlite3_free(esc);

        free(name);
        free(author);
    }
    free(line);
    fclose(fp);
}

int main(int argc, char **argv) {
    printf("create table if not exists REQUESTS (NAME, COUNT);\n");

    table("quotes.txt", "QUOTES");
    table("family.txt", "FAMILY");
    faq("faq.txt");

    return 0;
}
