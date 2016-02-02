#
#  $Id: Makefile.am,v 1.3 2004/02/26 11:38:35 kassatki Exp $
#
#  process this file with automake to produce Makefile.in
#

CLEANFILES = btsrv-parser.c btsrv-parser.h btsrv-lexer.c

bin_PROGRAMS = btsrv

btsrv_SOURCES = btsrv.c btsrv.h btsrv-parser.y btsrv-parser.h btsrv-lexer.l
EXTRA_btsrv_SOURCES = btsrv-sdp.c
if CONFIG_AFFIX_SDP
btsrv_LDADD = btsrv-sdp.o
endif

LIBS = $(LDFLAGS_AFFIX_SDP) $(LDFLAGS_AFFIX_UTILS) $(LDFLAGS_AFFIX_CORE) @LIBS@
CPPFLAGS = @CPPFLAGS@ -DBTDEBUG_MODULE=BTDEBUG_MODULE_CONTROL
YFLAGS = -d

