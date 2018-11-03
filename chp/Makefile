CC      = gcc
CFLAGS  = -g3 -std=c99 -pedantic -Wall
OBJDIR  = ~/Documents/ADCO/chp/bin/obj
SRCDIR  = ~/Documents/ADCO/chp/src
EXECDIR = ~/Documents/ADCO/chp/bin

all: cartographer check_types main zcc

cartographer: $(SRCDIR)/cartographer.c
	$(CC) -c -o $(OBJDIR)/$@.o $(CFLAGS) $^

check_types: $(SRCDIR)/check_types.c
	$(CC) -c -o $(OBJDIR)/$@.o $(CFLAGS) $^

main: $(SRCDIR)/main.c
	$(CC) -c -o $(OBJDIR)/$@.o $(CFLAGS) $^

zcc: $(OBJDIR)/chp.o $(OBJDIR)/cartographer.o $(OBJDIR)/check_types.o $(OBJDIR)/main.o
	$(CC) -o $(EXECDIR)/$@ $(CFLAGS) $^

clean:
	rm -f $(EXECDIR)/zcc $(OBJDIR)/cartographer.o $(OBJDIR)/check_types.o $(OBJDIR)/main.o $(SRCDIR)/*~
