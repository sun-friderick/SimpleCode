#
# $Id: Makefile,v 1.13 2004/02/22 19:57:00 kassatki Exp $
#
# Makefile for affix.o module
#

ifndef src
    src = .
endif

include $(src)/../config.mk

MODULES-y := affix.o

affix-y = hci.o af_hci.o hci_sched.o hci_mgr.o
affix-$(CONFIG_SYSCTL) += sysctl-core.o
affix-$(CONFIG_AFFIX_DEBUG) += btdebug.o
affix-$(CONFIG_AFFIX_L2CAP) += l2cap.o af_l2cap.o

INSTDIR := $(DESTDIR)$(MODDIR)/kernel/net/affix

include $(src)/../rules.mk

