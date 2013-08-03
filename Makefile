DRIVER_VERSION = $(shell sed -n 's/^\#define[ \t]\+DRIVER_VERSION[ \t]\+"\([^"]\+\)"/\1/p' driver/ndiswrapper.h)

UTILS_VERSION = $(shell sed -n 's/^\#define[ \t]\+UTILS_VERSION[ \t]\+"\([^"]\+\)"/\1/p' driver/ndiswrapper.h)

distdir=ndiswrapper-${DRIVER_VERSION}
distarchive=${distdir}.tar.gz

DISTFILES=AUTHORS ChangeLog INSTALL Makefile README ndiswrapper.spec \
				   ndiswrapper.8 loadndisdriver.8
SUBDIRS = utils driver

ifeq ($(wildcard /usr/share/man)$(wildcard /usr/man),/usr/man)
mandir = /usr/man
else
mandir = /usr/share/man
endif

KVERS ?= $(shell uname -r)

.PHONY: all
all: $(SUBDIRS)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

.PHONY: install
install:
	$(MAKE) -C driver install
	$(MAKE) -C utils install
	mkdir -p -m 755 $(DESTDIR)$(mandir)/man8
	install -m 644 ndiswrapper.8 $(DESTDIR)$(mandir)/man8
	install -m 644 loadndisdriver.8 $(DESTDIR)$(mandir)/man8

.PHONY: clean distclean
clean:
	$(MAKE) -C driver clean
	$(MAKE) -C utils clean
	rm -f *~
	rm -fr ${distdir} ${distdir}.tar.gz patch-stamp

distclean: clean
	$(MAKE) -C driver distclean
	$(MAKE) -C utils distclean
	rm -f .\#*

uninstall:
	rm -f $(DESTDIR)$(mandir)/man8/ndiswrapper.8
	rm -f $(DESTDIR)$(mandir)/man8/loadndisdriver.8
	$(MAKE) -C driver uninstall
	$(MAKE) -C utils uninstall

dist:
	@rm -rf ${distdir}
	mkdir -p ${distdir}
	@for file in $(DISTFILES); do \
	  cp $$file $(distdir)/$$file || exit 1; \
	done
	for subdir in $(SUBDIRS); do \
	  if test "$$subdir" = .; then :; else \
	    test -d $(distdir)/$$subdir \
	    || mkdir $(distdir)/$$subdir \
	    || exit 1; \
	  fi; \
	done
	$(MAKE) -C driver distdir=../${distdir}/driver dist
	$(MAKE) -C utils distdir=../${distdir}/utils dist

	# Update version in dist rpm spec file
	sed "s/\%define\s\+ndiswrapper_version\s\+[^\}]\+\}/%define ndiswrapper_version $(DRIVER_VERSION)\}/" \
		ndiswrapper.spec >$(distdir)/ndiswrapper.spec
	tar cfz ${distarchive} ${distdir}

rpm: dist ndiswrapper.spec
	rpmbuild -ta $(distarchive) --define="ndiswrapper_version $(DRIVER_VERSION)"
