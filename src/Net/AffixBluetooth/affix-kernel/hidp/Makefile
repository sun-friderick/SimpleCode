#
# $Id: Makefile,v 1.1 2004/06/02 10:14:39 hebben Exp $
#
# Makefile for affix_hidp.o module
#

ifndef src
    src = .
endif

include $(src)/../config.mk

MODULES-y := affix_hidp.o

affix_hidp-y = affix_hid.o hid.o
#affix_pan-$(CONFIG_SYSCTL) += sysctl-pan.o

INSTDIR := $(DESTDIR)$(MODDIR)/kernel/net/affix

include $(src)/../rules.mk

