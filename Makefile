#!/usr/bin/env -vS make -f 

PREFIX?=/usr/local

STATIC= -static 
LDFLAGS+= $(STATIC) 

CC?= cc 

OPTIMISATION+= -Og -mtune=generic 
DEBUGGING= -ggdb3 -fverbose-asm 
VERBOSITY= -v 
WARNINGS= -Wall 
CFLAGS+= $(OPTIMISATION) $(DEBUGGING) $(WARNINGS) $(VERBOSITY) 

INCLUDES+= -I/usr/include 
DEFINES+= 
CPPFLAGS+= $(DEFINES) $(INCLUDES) 

CSOURCES= xsplit.c xassem.c

all: bin/xsplit bin/xassem

bin/xsplit: Makefile xsplit.c
	mkdir -p bin >/dev/null 2>/dev/null
	$(CC) $(CFLAGS) xsplit.c $(LDFLAGS) -o bin/xsplit

bin/xassem: Makefile xassem.c
	mkdir -p bin >/dev/null 2>/dev/null
	$(CC) $(CFLAGS) xassem.c $(LDFLAGS) -o bin/xassem

clean:
	rm -f bin/xsplit bin/xassem >/dev/null 2>/dev/null

install: all
	mkdir -p -v "$(PREFIX)"/bin/
	install -p -m 0755 -v bin/xsplit bin/xassem "$(PREFIX)"/bin/
