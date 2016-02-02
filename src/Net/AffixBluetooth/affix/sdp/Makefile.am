#
#  $Id: Makefile.am,v 1.26 2004/02/27 17:24:29 kassatki Exp $
#
#  process this file with automake to produce Makefile.in
#

bin_PROGRAMS = btsdpd
lib_LTLIBRARIES = libaffix_sdp.la libaffix_sdpsvc.la

# SDP server options
btsdpd_SOURCES = sdpserver.c servicedb.c servicedb.h request.c
btsdpd_LDADD = libaffix_sdp.la libaffix_sdpsvc.la

# SDP library options
libaffix_sdp_la_SOURCES = attr.c attr.h uuid.c des.c utils.c utils.h sdpclt.c cstate.c cstate.h
libaffix_sdp_la_LDFLAGS = $(VERSION_AFFIX_LIBS)

libaffix_sdpsvc_la_SOURCES = sdpsrv.c
libaffix_sdpsvc_la_LDFLAGS = $(VERSION_AFFIX_LIBS)
libaffix_sdpsvc_la_LIBADD = libaffix_sdp.la

LIBS = $(LDFLAGS_AFFIX_CORE) @LIBS@
CPPFLAGS = @CPPFLAGS@ -DBTDEBUG_MODULE=BTDEBUG_MODULE_SDP

