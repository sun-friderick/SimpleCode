# 
#   $Id: Makefile,v 1.14 2003/08/13 13:06:35 jh Exp $
#
#   Makefile for compiling and installing the Affix stack
#
#
#

ifndef src
    src = .
endif

include $(src)/../config.mk

HEADERS = hci_types.h bluetooth.h hci_cmds.h

ifdef CONFIG_AFFIX_L2CAP_PACKET
HEADERS += packet.h
endif

all:
	@ln -fns ../include affix
	@mkdir -p linux
	@ln -sf ../config.h linux/config.h

clean:
	@rm -rf affix linux

distclean:	clean

install:
	install -m 0755 -d $(DESTDIR)/usr/include/affix
	for file in $(HEADERS) ; do \
		diff -q $$file $(DESTDIR)/usr/include/affix/$$file || \
			install -m 0644 $$file $(DESTDIR)/usr/include/affix; \
	done

uninstall:
	@CDIR=`pwd`;\
	cd $(DESTDIR)/usr/include/affix;\
	rm -f $(HEADERS);\
	cd $$CDIR
