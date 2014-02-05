# Use config.mak to override any of the following variables.
# Do not make changes here.

CC               ?= clang
CFLAGS            = -std=c11 -D_GNU_SOURCE -Wall -ggdb3 -DHAS_SSL
LDFLAGS           = -ldl -lrt -lpthread -lsqlite3 -lssl -lcrypto -Wl,--export-dynamic
SOURCES           = $(shell echo *.c)
OBJECTS           = $(SOURCES:.c=.o)
REDROID           = redroid
MODULE_CFLAGS     = -fPIC -std=gnu99 -Imodules/ -ggdb3
MODULE_LDFLAGS    = -shared -rdynamic -lm
MODULE_SOURCES    = $(shell echo modules/*.c)
MODULE_OBJECTS    = $(MODULE_SOURCES:.c=.so)
WHITELIST_CFLAGS  = -std=gnu99 -ggdb3
WHITELIST_LDFLAGS = -lsqlite3
WHITELIST_SOURCES = misc/whitelist.c
WHITELIST_OBJECTS = $(WHITELIST_SOURCES:.c=.o)

-include config.mak

all: $(SOURCES) $(MODULE_OBJECTS) $(REDROID) whitelist

$(REDROID): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

timestamp.o:
	$(CC) $(CFLAGS) -c -o timestamp.o timestamp.c

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

modules/%.so: modules/%.c
	$(CC) $(MODULE_CFLAGS) $(MODULE_LDFLAGS) $< -o $@

wlgen: $(WHITELIST_OBJECTS)
	$(CC) $(WHITELIST_LDFLAGS) $(WHITELIST_OBJECTS) -o $@

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
