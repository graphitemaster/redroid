SOURCES        != echo *.c
HEADERS        != echo *.h
OBJECTS         = ${SOURCES:S/.c/.o/g}
MODULE_SOURCES != echo modules/*.c
MODULE_OBJECTS  = ${MODULE_SOURCES:S/.c/.so/g}
WHITELIST_SOURCES = misc/whitelist.c
WHITELIST_OBJECTS = ${WHITELIST_SOURCES:S/.c/.o/g}

OSNAME         != uname -o

.if ${OSNAME:M*BSD} == ""
OS_LIBS = -ldl
.else
OS_LIBS =
.endif

include Makefile
