#
#  $Id: Makefile.am,v 1.17 2004/02/26 10:37:13 kassatki Exp $
#
#  process this file with automake to produce Makefile.in
#

bin_PROGRAMS = btobex
lib_LTLIBRARIES = libaffix_obex.la

# OBEX library options
libaffix_obex_la_SOURCES = obex_client.c obex_server.c
libaffix_obex_la_LDFLAGS = $(VERSION_AFFIX_LIBS)
libaffix_obex_la_LIBADD = $(LDFLAGS_AFFIX_SDP)

# OBEX server options
btobex_SOURCES = btobex.c
#btobex_LDADD = libaffix_obex.la
btobex_LDADD = $(LDFLAGS_AFFIX_OBEX)

LIBS = $(LDFLAGS_AFFIX_CORE) $(LDFLAGS_OPENOBEX) @LIBS@
CPPFLAGS = $(CPPFLAGS_OPENOBEX) @CPPFLAGS@ -DBTDEBUG_MODULE=BTDEBUG_MODULE_OBEX

