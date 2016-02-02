-include config.mk

all:
	@$(MAKE) -f Makefile .prereq.ok
	@$(MAKE) build

build:
	$(MAKE) -C etc all
	$(MAKE) -C include all
	$(MAKE) -C $(LINUX) SUBDIRS=$$PWD modules

clean:
	$(MAKE) -C $(LINUX) SUBDIRS=$$PWD clean
	$(MAKE) -f Makefile clean

distclean: clean
	$(MAKE) -f Makefile distclean

install:
	$(MAKE) -f Makefile install

uninstall:
	$(MAKE) -f Makefile uninstall

config:
	$(MAKE) -f Makefile config

dist:
	$(MAKE) -f Makefile dist

