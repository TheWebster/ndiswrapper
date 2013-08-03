sbindir = /sbin
usrsbindir = /usr/sbin

DRIVER_DIR ?= ../driver

CC = gcc
CFLAGS = -g -Wall -I$(DRIVER_DIR)

DISTFILES=Makefile ndiswrapper loadndisdriver.c ndiswrapper-buginfo

all: loadndisdriver

loadndisdriver: loadndisdriver.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f *~ *.o loadndisdriver

distclean: clean
	rm -f .\#*

install: all
	mkdir -p -m 755 $(DESTDIR)$(sbindir)
	mkdir -p -m 755 $(DESTDIR)$(usrsbindir)
	install -m 755 loadndisdriver $(DESTDIR)$(sbindir)
	install -m 755 ndiswrapper $(DESTDIR)$(usrsbindir)
	install -m 755 ndiswrapper-buginfo $(DESTDIR)$(usrsbindir)

uninstall:
	rm -f $(DESTDIR)$(sbindir)/loadndisdriver
	rm -f $(DESTDIR)$(usrsbindir)/ndiswrapper
	rm -f $(DESTDIR)$(usrsbindir)/ndiswrapper-buginfo

dist:
	@for file in $(DISTFILES); do \
	  cp $$file $(distdir)/$$file || exit 1; \
	done
