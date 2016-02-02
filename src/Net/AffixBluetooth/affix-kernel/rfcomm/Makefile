#
# $Id: Makefile,v 1.9 2004/02/22 19:57:00 kassatki Exp $
#
# Makefile for affix_rfcomm.o module
#

ifndef src
    src = .
endif

include $(src)/../config.mk

MODULES-y := affix_rfcomm.o

affix_rfcomm-y = rfcomm.o af_rfcomm.o bty.o
affix_rfcomm-$(CONFIG_SYSCTL) += sysctl-rfcomm.o

INSTDIR := $(DESTDIR)$(MODDIR)/kernel/net/affix

include $(src)/../rules.mk

