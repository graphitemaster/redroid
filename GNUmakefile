SOURCES           = $(wildcard *.c)
HEADERS           = $(wildcard *.h)
OBJECTS           = $(SOURCES:.c=.o)
MODULE_SOURCES    = $(wildcard modules/*.c)
MODULE_OBJECTS    = $(MODULE_SOURCES:.c=.so)
WHITELIST_SOURCES = misc/whitelist.c
WHITELIST_OBJECTS = $(WHITELIST_SOURCES:.c=.o)

OSNAME           := $(shell uname -o)

ifeq (,$(findstring BSD,$(OSNAME)))
OS_LIBS = -ldl
else
OS_LIBS =
endif

include Makefile
