CC      = gcc
CFLAGS  = -g3 -std=c99 -pedantic -Wall
OBJDIR  = ~/Documents/ADCO/chp/bin/obj
SRCDIR  = ~/Documents/ADCO/chp/src
EXECDIR = ~/Documents/ADCO/chp/bin

all: ADCO zcc

ADCO: $(SRCDIR)/ADCO.c
	$(CC) -c -o $(OBJDIR)/$@.o $(CFLAGS) $^

zcc: $(OBJDIR)/chp.o $(OBJDIR)/ADCO.o
	$(CC) -o $(EXECDIR)/$@ $(CFLAGS) $^

clean:
	rm -f $(EXECDIR)/zcc $(OBJDIR)/ADCO.o $(SRCDIR)/*~
