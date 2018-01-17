#**************************************************
#*                  Galen Helfter
#*               ghelfter@gmail.com
#*                     Makefile
#**************************************************

SHELL = /bin/sh

PROJ = batch_collect

# Compiler and linker flags
CC = gcc
CFLAGS = -g -W -Wall -Wdeclaration-after-statement \
		 -pedantic -Wextra -O0 -std=c89 $(INC)
LDFLAGS = -g -W -Wall -Wdeclaration-after-statement \
		  -pedantic -Wextra -O0 -std=c89 $(LIB)

LIBS = -lssh -lcjson

TARFILE = $(PROJ).tar.gz
ZIPFILE = $(PROJ).zip


CLEANFILES = *.o *.out core.* $(TARFILE) $(ZIPFILE)

# Object file list
OBJECTS =	main.o \
			ssh_session.o

# Programs

PROGS = run

all:		$(PROGS)

run:		$(OBJECTS) $(LIBS)
			$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)


.PHONY: clean tar zip
clean:
		rm -f $(PROGS) $(CLEANFILES)
tar:
		rm -f $(TARFILE)
		tar -czvf $(TARFILE) ./*
zip:
		rm -f $(ZIPFILE)
		zip $(ZIPFILE) ./*
