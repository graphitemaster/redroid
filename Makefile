CC     ?= clang
CFLAGS  = -std=gnu99 -Wall -ggdb3
LDFLAGS = -ldl -Wl,--export-dynamic
SOURCES = irc.c ircman.c list.c main.c module.c sock.c ini.c config.c
OBJECTS = $(SOURCES:.c=.o)
REDROID = redroid

MODULE_CFLAGS  = -fPIC -std=gnu99 -Imodules/ -ggdb3
MODULE_LDFLAGS = -shared -rdynamic -lm
MODULE_SOURCES = $(shell echo modules/*.c)
MODULE_OBJECTS = $(MODULE_SOURCES:.c=.so)

all: $(SOURCES) $(MODULE_OBJECTS) $(REDROID)

$(REDROID): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

modules/%.so: modules/%.c
	$(CC) $(MODULE_CFLAGS) $(MODULE_LDFLAGS) $< -o $@

modules: $(MODULE_OBJECTS)

cleanmodules:
	rm -f $(MODULE_OBJECTS)

clean: cleanmodules
	rm -f $(OBJECTS)
	rm -f $(REDROID)
