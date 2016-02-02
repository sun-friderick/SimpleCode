#
#  $Id: Makefile.am,v 1.25 2004/03/03 07:49:26 kassatki Exp $
#
#  process this file with automake to produce Makefile.in
#

if CONFIG_AFFIX_PAN
btpan_PROG = btpan
endif

if CONFIG_AFFIX_OBEX
btftp_PROG = btftp
endif

bin_PROGRAMS = bthidctl btctl btclt btsocket $(btftp_PROG) $(btpan_PROG)

# check conditions
if CONFIG_AFFIX_AUDIO
btctl_AUDIO = btctl-audio.o
endif

if CONFIG_AFFIX_SDP
btclt_SDP = btsdp.o
endif

btctl_SOURCES = btctl.c btctl.h btctl-dev.c
btctl_LDADD = $(btctl_AUDIO)

btclt_SOURCES = btclt.c
btclt_LDADD = $(btclt_SDP) $(LDFLAGS_AFFIX_SDP) $(LDFLAGS_AFFIX_UTILS)

btsocket_SOURCES = btsocket.c
btsocket_LDADD = $(LDFLAGS_AFFIX_UTILS)

btftp_SOURCES = btftp.c
btftp_LDADD = $(LDFLAGS_AFFIX_UTILS) $(LDFLAGS_AFFIX_OBEX) $(LDFLAGS_READLINE)

btpan_SOURCES = btpan.c
btpan_LDADD = $(LDFLAGS_AFFIX_SDP)

bthidctl_SOURCES = bthidctl.c
bthidctl_LDADD = $(LDFLAGS_AFFIX_SDP)

EXTRA_btctl_SOURCES = btctl-audio.c btsdp.c #btclt.c btftp.c btpan.c

LIBS =  $(LDFLAGS_AFFIX_CORE) @LIBS@
CPPFLAGS = $(CPPFLAGS_AFFIX_OBEX) @CPPFLAGS@ -DBTDEBUG_MODULE=BTDEBUG_MODULE_CONTROL
