# Use config.mak to override any of the following variables.
# Do not make changes here.

CC               ?= clang
CFLAGS            = -std=c11 -D_XOPEN_SOURCE=700 -Wall -Wextra -ggdb3 -DHAS_SSL
LDFLAGS           = -ldl -lrt -lpthread -lsqlite3 -lgnutls -Wl,--export-dynamic
SOURCES           = $(wildcard *.c)
HEADERS           = $(wildcard *.h)
OBJECTS           = $(SOURCES:.c=.o)
REDROID           = redroid
MODULE_CFLAGS     = -fPIC -fno-asm -fno-builtin -std=c99 -Wall -Wextra -D_POSIX_SOURCE -Imodules/ -ggdb3
MODULE_LDFLAGS    = -shared -rdynamic -lm
MODULE_SOURCES    = $(wildcard modules/*.c)
MODULE_OBJECTS    = $(MODULE_SOURCES:.c=.so)
WHITELIST_CFLAGS  = -std=gnu99 -ggdb3
WHITELIST_LDFLAGS = -lsqlite3
WHITELIST_SOURCES = misc/whitelist.c
WHITELIST_OBJECTS = $(WHITELIST_SOURCES:.c=.o)
LAMBDAPP          = lambdapp/lambdapp
STRIP             = $(shell strip)

-include config.mak

all: modules $(REDROID) whitelist

$(LAMBDAPP):
	cd lambdapp && $(MAKE)

$(REDROID): $(LAMBDAPP) $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

timestamp.o:
	$(CC) $(CFLAGS) -c -o timestamp.o timestamp.c

.c.o:
	$(LAMBDAPP) $< | $(CC) -xc -c $(CFLAGS) - -o $@

modules/%.so: modules/%.c
	$(CC) $(MODULE_CFLAGS) $< -o $@ $(MODULE_LDFLAGS)

wlgen: $(WHITELIST_OBJECTS)
	$(CC) $(WHITELIST_OBJECTS) -o $@ $(WHITELIST_LDFLAGS)

whitelist: wlgen
	@./wlgen

modules: $(MODULE_OBJECTS)

install: $(REDROID) modules
	install -d -m755 /usr/local/redroid
	install -d -m755 /usr/local/redroid/modules
	install -d -m755 /usr/local/redroid/site
	install -d -m755 /usr/local/redroid/misc
	install -m755 $(SOURCES)           /usr/local/redroid
	install -m755 $(HEADERS)           /usr/local/redroid
	install -m755 $(MODULE_SOURCES)    /usr/local/redroid/modules
	install -m755 $(MODULE_OBJECTS)    /usr/local/redroid/modules
	install -m755 modules/module.h     /usr/local/redroid/modules/module.h
	install -m755 $(REDROID)           /usr/local/redroid/$(REDROID)
	install -m755 whitelist.db         /usr/local/redroid/whitelist.db
	install -m755 config.ini           /usr/local/redroid/config.ini
	install -m755 database.db          /usr/local/redroid/database.db
	install -m755 site/*               /usr/local/redroid/site
	install -m755 Makefile             /usr/local/redroid/Makefile
	install -m755 misc/whitelist.c     /usr/local/redroid/misc/whitelist.c
	install -m755 misc/whitelist       /usr/local/redroid/misc/whitelist

install-systemd-service:
	install -m755 misc/redroid.service /etc/systemd/system/redroid.service

install-all: install install-systemd-service

uninstall-systemd-service:
	rm -f /etc/systemd/system/redroid.service

uninstall:
	rm -rf /usr/local/redroid

uninstall-all: uninstall uninstall-systemd-service

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
