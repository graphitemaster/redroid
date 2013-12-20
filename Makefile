CC     ?= clang
CFLAGS  = -std=gnu99 -Wall -ggdb3
LDFLAGS = -ldl
SOURCES = irc.c ircman.c list.c main.c module.c sock.c ini.c config.c
OBJECTS = $(SOURCES:.c=.o)
REDROID = redroid

MODULE_CFLAGS  = -fPIC -std=gnu99
MODULE_LDFLAGS = -shared -rdynamic
MODULE_SOURCES = $(shell echo modules/*.c)
MODULE_OBJECTS = $(MODULE_SOURCES:.c=.so)

all: $(SOURCES) $(MODULE_OBJECTS) $(REDROID)

$(REDROID): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

modules/%.so: $(MODULE_SOURCES)
	$(CC) $(MODULE_CFLAGS) $(MODULE_LDFLAGS) $< -o $@


clean:
	rm -f $(OBJECTS)
	rm -f $(REDROID)
	rm -f $(MODULE_OBJECTS)
