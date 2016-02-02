#
# $Id: Makefile,v 1.5 2004/02/22 20:20:20 kassatki Exp $
#
# Makefile for driver modules
#

ifndef src
    src = .
endif

include $(src)/../config.mk

MODULES-$(CONFIG_AFFIX_USB_MODULE) += affix_usb.o
MODULES-$(CONFIG_AFFIX_UART_MODULE) += affix_uart.o
MODULES-$(CONFIG_AFFIX_UART_CS_MODULE) += affix_uart_cs.o
MODULES-$(CONFIG_AFFIX_BLUECARD_CS_MODULE) += affix_bluecard_cs.o
MODULES-$(CONFIG_AFFIX_BT3C_CS_MODULE) += affix_bt3c_cs.o
MODULES-$(CONFIG_AFFIX_BT950UART_CS_MODULE) += affix_bt950uart_cs.o

affix_usb-objs = btusb.o

affix_uart-y = btuart.o btuart_tty.o
affix_uart-$(CONFIG_AFFIX_UART_H4) += btuart_h4.o
affix_uart-$(CONFIG_AFFIX_UART_TLP) += btuart_tlp.o
affix_uart-$(CONFIG_AFFIX_UART_BCSP) += btuart_bcsp.o

affix_uart_cs-objs += btuart_cs.o

affix_bluecard_cs-objs += bluecard_cs.o
affix_bt3c_cs-objs += bt3c_cs.o
affix_bt950uart_cs-objs += bt950uart_cs.o

INSTDIR := $(DESTDIR)$(MODDIR)/kernel/drivers/affix

include $(src)/../rules.mk

