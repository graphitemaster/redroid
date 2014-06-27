#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "string.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 8
#define VERSION_PATCH 0

#define BUILD_SECOND (__TIME__[6] * 10 + __TIME__[7] - 528)
#define BUILD_MINUTE (__TIME__[3] * 10 + __TIME__[4] - 528)
#define BUILD_HOUR   (__TIME__[0] * 10 + __TIME__[1] - 528)

#define BUILD_DAY   (__DATE__[4] * 10 + __DATE__[5] - (__DATE__[4] == ' ' ? 368 : 528))
#define BUILD_YEAR  (__DATE__[7] * 1000 + __DATE__[8] * 100 + __DATE__[9] * 10 + __DATE__[10] - 53328)

#define BUILD_MONTH ((__DATE__[1]+__DATE__[2] == 207) ? 1  : (__DATE__[1]+__DATE__[2] == 199) ? 2  : \
                     (__DATE__[1]+__DATE__[2] == 211) ? 3  : (__DATE__[1]+__DATE__[2] == 226) ? 4  : \
                     (__DATE__[1]+__DATE__[2] == 218) ? 5  : (__DATE__[1]+__DATE__[2] == 227) ? 6  : \
                     (__DATE__[1]+__DATE__[2] == 225) ? 7  : (__DATE__[1]+__DATE__[2] == 220) ? 8  : \
                     (__DATE__[1]+__DATE__[2] == 213) ? 9  : (__DATE__[1]+__DATE__[2] == 215) ? 10 : \
                     (__DATE__[1]+__DATE__[2] == 229) ? 11 : (__DATE__[1]+__DATE__[2] == 200) ? 12 : 0)

static string_t *build_string = NULL;

static void build_stamp_free(void) {
    string_destroy(build_string);
}

static string_t *build_stamp_create(void) {
    if (!build_string) {
        struct tm stamp = {
            .tm_sec  = BUILD_SECOND,
            .tm_min  = BUILD_MINUTE,
            .tm_mday = BUILD_DAY,
            .tm_hour = BUILD_HOUR  - 1,
            .tm_mon  = BUILD_MONTH - 1,
            .tm_year = BUILD_YEAR  - 1900
        };

        mktime(&stamp);
        char buffer[256];
        strftime(buffer, sizeof(buffer), "%B %d, %Y %I:%M %p", &stamp);
        build_string = string_format("Redroid v%d.%d.%d (%s)",
                                     VERSION_MAJOR,
                                     VERSION_MINOR,
                                     VERSION_PATCH,
                                     buffer);
        atexit(build_stamp_free);
    }
    return build_string;
}

const char *redroid_buildinfo() {
    return string_contents(build_stamp_create());
}
