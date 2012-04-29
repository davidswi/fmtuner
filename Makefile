# File: makefile
# Author: David Switzer
# Project: FmTuner, WebKit-based FM tuner UI
# (c) 2012, David Switzer

SRC = fmdriverif.c
OBJ = $(SRC:.c=.o)
TUNERLIB = lib/FMTuner.a
INCLUDES = -I. -I../inc -I/usr/include
CC = gcc
CFLAGS = -g -O2 -Wall
LDFLAGS = -g

.SUFFIXES: .c

default: $(TUNERLIB)

.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) -c $< -o $@

$(TUNERLIB): $(OBJ)
	ar rcs $(TUNERLIB) $(OBJ)

clean:
	rm -f $(TUNERLIB) $(OBJ) Makefile.bak 

