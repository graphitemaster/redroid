# Use config.mak to override any of the following variables.
# Do not make changes here.

CC               ?= clang
CFLAGS            = -std=c11 -D_XOPEN_SOURCE=700 -Wall -ggdb3 -DHAS_SSL
LDFLAGS           = -ldl -lrt -lpthread -lsqlite3 -lssl -lcrypto -Wl,--export-dynamic
SOURCES           = $(shell echo *.c)
OBJECTS           = $(SOURCES:.c=.o)
REDROID           = redroid
MODULE_CFLAGS     = -fPIC -fno-asm -fno-builtin -std=c99 -D_POSIX_SOURCE -Imodules/ -ggdb3
MODULE_LDFLAGS    = -shared -rdynamic -lm
MODULE_SOURCES    = $(shell echo modules/*.c)
MODULE_OBJECTS    = $(MODULE_SOURCES:.c=.so)
WHITELIST_CFLAGS  = -std=gnu99 -ggdb3
WHITELIST_LDFLAGS = -lsqlite3
WHITELIST_SOURCES = misc/whitelist.c
WHITELIST_OBJECTS = $(WHITELIST_SOURCES:.c=.o)
STRIP             = $(shell strip)

-include config.mak

all: modules $(REDROID) whitelist

$(REDROID): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

timestamp.o:
	$(CC) $(CFLAGS) -c -o timestamp.o timestamp.c

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

modules/%.so: modules/%.c
	$(CC) $(MODULE_CFLAGS) $(MODULE_LDFLAGS) $< -o $@

wlgen: $(WHITELIST_OBJECTS)
	$(CC) $(WHITELIST_OBJECTS) -o $@ $(WHITELIST_LDFLAGS)

whitelist: wlgen
	@./wlgen

modules: $(MODULE_OBJECTS)

cleanmodules:
	rm -f $(MODULE_OBJECTS)

cleanwlgen:
	rm -f wlgen

cleanwhitelist:
	rm -f whitelist.db

clean: cleanmodules cleanwlgen
	rm -f $(OBJECTS)
	rm -f $(REDROID)

.PHONY: timestamp.o
