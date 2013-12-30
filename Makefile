CC      ?= clang
CFLAGS   = -std=gnu99 -Wall -ggdb3
LDFLAGS  = -ldl -lpthread -lsqlite3 -Wl,--export-dynamic
SOURCES  = $(shell echo *.c)
OBJECTS  = $(SOURCES:.c=.o)
REDROID  = redroid

MODULE_CFLAGS  = -fPIC -std=gnu99 -Imodules/ -ggdb3
MODULE_LDFLAGS = -shared -rdynamic -lm
MODULE_SOURCES = $(shell echo modules/*.c)
MODULE_OBJECTS = $(MODULE_SOURCES:.c=.so)

all: $(SOURCES) $(MODULE_OBJECTS) $(REDROID)

$(REDROID): $(OBJECTS) whitelist
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

modules/%.so: modules/%.c
	$(CC) $(MODULE_CFLAGS) $(MODULE_LDFLAGS) $< -o $@

whitelist: cleanwhitelist
	@echo Creating module whitelist database
	@$(CC) misc/whitelist.c -o misc/gen
	@./misc/gen
	@rm -f misc/gen

modules: $(MODULE_OBJECTS)

cleanmodules:
	rm -f $(MODULE_OBJECTS)

cleanwhitelist:
	rm -f whitelist.db

clean: cleanmodules cleanwhitelist
	rm -f $(OBJECTS)
	rm -f $(REDROID)
