#!/usr/bin/make

CC	= gcc
CFLAGS  ?= -g
LIBDIR  ?= /usr/lib

all:
	$(CC) $(CFLAGS) -shared -fPIC -o oom-score-adj.so slurm-spank-oom-score-adj.c

install: all
	mkdir -p $(DESTDIR)/$(LIBDIR)/slurm
	install -m 755 oom-score-adj.so $(DESTDIR)$(LIBDIR)/slurm/

clean:
	rm -f oom-score-adj.so

