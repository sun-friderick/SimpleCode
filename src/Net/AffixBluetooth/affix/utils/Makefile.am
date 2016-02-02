#
#  $Id: Makefile.am,v 1.3 2004/03/02 14:21:30 kassatki Exp $
#
#  process this file with automake to produce Makefile.in
#

EXTRA_DIST = btmpg123

bin_PROGRAMS = btmodem
lib_LTLIBRARIES = libaffix_utils.la

# SDP server options
btmodem_SOURCES = btmodem.c
#btmodem_LDADD = xxx.o

libaffix_utils_la_SOURCES = btutils.c
libaffix_utils_la_LDFLAGS = $(VERSION_AFFIX_LIBS)
libaffix_utils_la_LIBADD = $(LDFLAGS_AFFIX_SDP)

LIBS = $(LDFLAGS_AFFIX_CORE) @LIBS@
CPPFLAGS = @CPPFLAGS@ -DBTDEBUG_MODULE=BTDEBUG_MODULE_TOOLS

