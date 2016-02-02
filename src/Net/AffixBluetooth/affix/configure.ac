#
#  $Id: configure.ac,v 1.82 2004/07/20 11:48:29 chineape Exp $
#
#  process this file with autoconf to produce a configure script.
#

AC_INIT(Affix, 3.2.0, affix-devel@lists.sourceforge.net, affix)
AM_INIT_AUTOMAKE(affix, 3.2.0)
#AC_CONFIG_HEADERS(include/config.h)
AM_CONFIG_HEADER(include/config.h)
#prefix
AC_PREFIX_DEFAULT(/usr/local)
AC_DEFINE([CONFIG_AFFIX_PREFIX], [/usr/local], [Affix Prefix])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_YACC
#AC_PROG_LEX
AM_PROG_LEX

# check for headers
AC_CHECK_HEADERS(affix/bluetooth.h,, 
	[AC_MSG_ERROR([Affix header files not found. Please, install affix-kernel for that.])])

AFFIX_SUBDIRS=lib
AFFIX_EXTRA_SUBDIRS=
LDFLAGS_AFFIX_CORE="\$(top_builddir)/lib/libaffix.la"
LDFLAGS_AFFIX_SDP=""
LDFLAGS_AFFIX_OBEX=""
CPPFLAGS_AFFIX_OBEX=""
LDFLAGS_AFFIX_UTILS="\$(top_builddir)/utils/libaffix_utils.la"
LDFLAGS_READLINE=""
# current[:revision[:age]]
# result: [current-age].age.revision
VERSION_AFFIX_LIBS="-version-info 5:0:2"


AFFIX_ARG_ENABLE(audio, "yes", CONFIG_AFFIX_AUDIO, [Enable Audio support in Affix])
AFFIX_ARG_ENABLE(rfcomm, "yes", CONFIG_AFFIX_RFCOMM, [Enable RFCOMM support in Affix (required)])
AFFIX_ARG_ENABLE(pan, "yes", CONFIG_AFFIX_PAN, [Enable PAN support in Affix])
AFFIX_ARG_ENABLE(uart, "yes", CONFIG_AFFIX_UART, [Enable UART support in Affix])
AFFIX_ARG_ENABLE(sdp, "yes", CONFIG_AFFIX_SDP, [Enable SDP support in Affix])
AFFIX_ARG_ENABLE(obex, "yes", CONFIG_AFFIX_OBEX, [Enable OBEX support in Affix])
AFFIX_ARG_ENABLE(debug, "no", CONFIG_AFFIX_DEBUG, [Enable Debug support in Affix])
AFFIX_ARG_ENABLE(bt_1_2, "yes", CONFIG_AFFIX_BT_1_2, [Enable BLUETOOTH 1.2 support in Affix])



AM_CONDITIONAL(CONFIG_AFFIX_AUDIO, test $affix_cv_use_audio = yes)
AM_CONDITIONAL(CONFIG_AFFIX_RFCOMM, test $affix_cv_use_rfcomm = yes)
AM_CONDITIONAL(CONFIG_AFFIX_PAN, test $affix_cv_use_pan = yes)
AM_CONDITIONAL(CONFIG_AFFIX_UART, test $affix_cv_use_uart = yes)
AM_CONDITIONAL(CONFIG_AFFIX_BT_1_2, test $affix_cv_use_bt_1_2 = yes)


if test $affix_cv_use_sdp = yes; then
	AFFIX_SUBDIRS="$AFFIX_SUBDIRS sdp"
	LDFLAGS_AFFIX_SDP="\$(top_builddir)/sdp/libaffix_sdp.la \$(top_builddir)/sdp/libaffix_sdpsvc.la"
fi
AM_CONDITIONAL(CONFIG_AFFIX_SDP, test $affix_cv_use_sdp = yes)

# check OBEX
if test $affix_cv_use_obex = yes; then
	# check for libopenobex
	AFFIX_PATH_OPENOBEX(1.0.0)
	if test "$OPENOBEX_CONFIG" != "no" ; then
		CPPFLAGS_OPENOBEX="$OPENOBEX_CFLAGS"
		LDFLAGS_OPENOBEX="$OPENOBEX_LIBS"
	else
		AC_MSG_ERROR([OPENOBEX not found])
	fi
	AFFIX_SUBDIRS="$AFFIX_SUBDIRS obex"
	LDFLAGS_AFFIX_OBEX="\$(top_builddir)/obex/libaffix_obex.la $LDFLAGS_OPENOBEX"
	CPPFLAGS_AFFIX_OBEX="$CPPFLAGS_OPENOBEX"
	AC_DEFINE_UNQUOTED([OPENOBEX_VERSION_CODE],$openobex_version_code, [openobex version])

	AC_ARG_WITH(readline, [  --without-readline	without support funcy command editing for btftp])
	if test "$with_readline" != "no"; then
		XXLIBS="$LIBS"
		AC_CHECK_LIB(readline, readline)
		AC_CHECK_HEADERS(readline/readline.h,, 
			[AC_MSG_ERROR([readline header files not found. 
Please, install libreadline for that. or use --without-readline])])
		LIBS="$XXLIBS"
		LDFLAGS_READLINE="-lreadline"
fi

fi
AM_CONDITIONAL(CONFIG_AFFIX_OBEX, test $affix_cv_use_obex = yes)

if test $affix_cv_use_debug = yes; then
	CFLAGS="-g -O1 -Wall -Wstrict-prototypes -pipe"
else
	CFLAGS="-O2 -Wall -Wstrict-prototypes -fomit-frame-pointer -pipe"
fi
AM_CONDITIONAL(CONFIG_AFFIX_DEBUG, test $affix_cv_use_debug = yes)

AFFIX_SUBDIRS="$AFFIX_SUBDIRS utils daemon control etc include"

# base flags
CPPFLAGS="-D_GNU_SOURCE -I\$(top_srcdir)/include -include \$(top_srcdir)/include/config.h"
LIBS=""

includedir=/usr/include
sysconfdir=/etc
# check affix_sysconfdir
if test "$affix_sysconfdir" != "" ; then
	sysconfdir=$affix_sysconfdir
fi

#LIBTOOL="$LIBTOOL $affix_libtool"

# substitute it
#AC_SUBST(affix_sysconfdir)
AC_SUBST(cross_compiling)
AC_SUBST(AFFIX_SUBDIRS)
AC_SUBST(AFFIX_EXTRA_SUBDIRS)
AC_SUBST(CPPFLAGS_OPENOBEX)
AC_SUBST(LDFLAGS_OPENOBEX)
AC_SUBST(LDFLAGS_AFFIX_CORE)
AC_SUBST(LDFLAGS_AFFIX_SDP)
AC_SUBST(LDFLAGS_READLINE)
AC_SUBST(LDFLAGS_AFFIX_OBEX)
AC_SUBST(CPPFLAGS_AFFIX_OBEX)
AC_SUBST(LDFLAGS_AFFIX_UTILS)
AC_SUBST(VERSION_AFFIX_LIBS)

AC_CONFIG_LINKS(include/affix:include)

AC_CONFIG_FILES([Makefile
                 lib/Makefile
		 sdp/Makefile
		 obex/Makefile
		 utils/Makefile
		 daemon/Makefile
		 control/Makefile
		 etc/Makefile
		 etc/btsrv.conf
		 etc/affix.conf
		 include/Makefile
		 etc/affix.pc
		 ])
AC_OUTPUT
