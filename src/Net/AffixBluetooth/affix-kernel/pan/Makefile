#
# $Id: Makefile,v 1.13 2004/02/22 19:57:00 kassatki Exp $
#
# Makefile for affix_pan.o module
#

ifndef src
    src = .
endif

include $(src)/../config.mk

MODULES-y := affix_pan.o

affix_pan-y = pan.o bnep.o
affix_pan-$(CONFIG_SYSCTL) += sysctl-pan.o

INSTDIR := $(DESTDIR)$(MODDIR)/kernel/net/affix

include $(src)/../rules.mk

