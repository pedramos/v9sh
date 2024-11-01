# makefile for v9sh
TESTDIR = .
FRC =
ROOT=
INSDIR =
DEFINES = -D_POSIX_SOURCE -D_BSD_EXTENSION -D_RESEARCH_SOURCE # -DBSD4_2
CFLAGS = $(DEFINES) -O -w
LINTFLAGS=-ha $(DEFINES)
LDFLAGS =
LIBS=

CFILES = main.c stak.c cmd.c fault.c word.c string.c\
 name.c args.c xec.c service.c error.c io.c print.c macro.c expand.c\
 ctype.c msg.c defs.c pathserv.c func.c spname.c
OFILES = main.o stak.o cmd.o fault.o word.o string.o\
 name.o args.o xec.o service.o error.o io.o print.o macro.o expand.o\
 ctype.o msg.o defs.o pathserv.o func.o spname.o

all: sh

sh: $(SFILES) $(OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SFILES) $(OFILES) $(LIBS) -o $(TESTDIR)/sh
lint: $(CFILES)
	lint $(LINTFLAGS) $(CFILES) | grep -v ':$$'

$(OFILES): $(FRC) ctype.h defs.h mac.h mode.h name.h stak.h sym.h

test:
	rtest $(TESTDIR)/sh

install:  all
	mv /bin/sh /bin/osh
	cp sh /bin/sh
	/etc/chown bin,bin /bin/sh
	chmod o-w,g+w /bin/sh

clean:
	  -rm -f *.o sh core mon.out

clobber:  clean
	  -rm -f $(TESTDIR)/sh
	  -rm -f $(ROOT)/bin/OLDrsh

FRC:

pp:
	pp -tsh makefile *.h *.c | dcan
print: makefile *.h *.c
	# pr -l132 $? | impr -p
	pp $?
	touch print
