# Use config.mak to override any of the following variables.
# Do not make changes here.

CC               ?= clang
LAMBDA_CC         = lambdapp/lambda-cc
CFLAGS            = -pipe -std=c11 -D_XOPEN_SOURCE=700 -Wall -Wextra
LDFLAGS           = -ldl -lrt -lm -lpthread -lsqlite3 -Wl,--export-dynamic
SOURCES           = $(wildcard *.c)
HEADERS           = $(wildcard *.h)
OBJECTS           = $(SOURCES:.c=.o)
REDROID           = redroid
MODULE_CFLAGS     = -fPIC -fno-asm -fno-builtin -ffreestanding -nostdinc -std=c99 -Wall -Wextra -D_XOPEN_SOURCE=700 -Imodules/include
MODULE_LDFLAGS    = -shared -rdynamic
MODULE_SOURCES    = $(wildcard modules/*.c)
MODULE_OBJECTS    = $(MODULE_SOURCES:.c=.so)
STRIP             = $(shell strip)

ifeq ($(GNUTLS),1)
	CFLAGS += -DHAS_SSL
	LDFLAGS += -lgnutls
endif

ifeq ($(DEBUG),1)
	CFLAGS += -g3
	MODULE_CFLAGS += -g3
else
	CFLAGS += -fomit-frame-pointer -O3
	MODULE_CFLAGS += -fomit-frame-pointer -O3
endif

all: modules $(REDROID)

$(OBJECTS): $(LAMBDA_CC)

$(MODULE_OBJECTS): $(LAMBDA_CC)

$(LAMBDA_CC):
	cd lambdapp && $(MAKE)

$(REDROID): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

timestamp.o:
	$(LAMBDA_CC) -c $(CFLAGS) timestamp.c -o timestamp.o

.c.o:
	$(LAMBDA_CC) -c $(CFLAGS) $< -o $@

modules/%.so: modules/%.c
	$(LAMBDA_CC) $(MODULE_CFLAGS) $< -o $@ $(MODULE_LDFLAGS)

modules: $(MODULE_OBJECTS)

cleanmodules:
	rm -f $(MODULE_OBJECTS)

moduledocs:
	cd modules/docs && $(MAKE)

cleanmoduledocs:
	cd modules/docs && $(MAKE) clean

cleanlambdapp:
	cd lambdapp && $(MAKE) clean

clean: cleanmodules cleanlambdapp
	rm -f $(OBJECTS)
	rm -f $(REDROID)

.PHONY: timestamp.o
