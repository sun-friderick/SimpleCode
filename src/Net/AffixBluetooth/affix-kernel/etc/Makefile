#
# $Id: Makefile,v 1.15 2004/02/22 19:57:00 kassatki Exp $
#
# Makefile for config stuff
#

ifndef src
    src = .
endif

include $(src)/../config.mk

all:

clean:
	@rm -f *~

install:	nodes
	@echo "Installing scripts"
	install -m 0755 -d $(DESTDIR)/etc/affix
	
	install -m 0755 -d $(DESTDIR)/etc/pcmcia
	install -m 0644 pcmcia.conf $(DESTDIR)/etc/pcmcia/affix.conf

ifeq (, $(DESTDIR))
	@if [ -d /etc/modutils ] ; then \
		install -m 0644 ./modutils $(DESTDIR)/etc/modutils/affix; \
		update-modules; \
	fi
else
	install -m 0755 -d $(DESTDIR)/etc/modutils
	install -m 0644 ./modutils $(DESTDIR)/etc/modutils/affix
endif

uninstall:
	@CDIR=`pwd`;\
	cd $(DESTDIR)/etc/pcmcia;\
	rm -f affix.conf;\
	rm -f $(DESTDIR)/etc/modutils/affix;\
	cd $$CDIR
	
nodes:
ifdef CONFIG_DEVFS_FS
	@echo installing devfs conf file
	install -m 0755 -d $(DESTDIR)/etc/devfs/conf.d
	install -m 0644 devfs $(DESTDIR)/etc/devfs/conf.d/affix
else
	@echo Creating nodes in /dev/...
	install -m 0755 -d $(DESTDIR)/dev
	@rm -rf $(DESTDIR)/dev/bty0
	@mknod -m 0644 $(DESTDIR)/dev/bty0 c 60 0
	@rm -rf $(DESTDIR)/dev/bty1
	@mknod -m 0644 $(DESTDIR)/dev/bty1 c 60 1
	@rm -rf $(DESTDIR)/dev/bty2
	@mknod -m 0644 $(DESTDIR)/dev/bty2 c 60 2
	@rm -rf $(DESTDIR)/dev/bty3
	@mknod -m 0644 $(DESTDIR)/dev/bty3 c 60 3
	@rm -rf $(DESTDIR)/dev/bty4
	@mknod -m 0644 $(DESTDIR)/dev/bty4 c 60 4
endif

