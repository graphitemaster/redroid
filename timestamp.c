#include <stdio.h>

#define VERSION_MAJOR 0
#define VERSION_MINOR 8
#define VERSION_PATCH 0

const char *build_date() {
    static const char *compile_date = __DATE__;
    return compile_date;
}

const char *build_time() {
    static const char *compile_time = __TIME__;
    return compile_time;
}

const char *build_version() {
    static char table[128] = {0};
    if (!*table) {
        snprintf(table, sizeof(table),
            "Redroid v%d.%d.%d (built on %s at %s)",
            VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,
            build_date(), build_time()
        );
    }
    return table;
}
