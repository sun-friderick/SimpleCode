#
#  $Id: Makefile.am,v 1.31 2004/03/19 15:55:05 kassatki Exp $
#
#  process this file with automake to produce Makefile.in
#

affixconfdir = $(sysconfdir)/affix
pppupdir = $(sysconfdir)/ppp/ip-up.d
pppdowndir = $(sysconfdir)/ppp/ip-down.d
pkgconfigdir = /usr/lib/pkgconfig

affixconf_DATA = btsrv.conf affix.conf device.map services dhcpd.conf pan.conf
affixconf_SCRIPTS = serial btsrv-gui affix
pppup_SCRIPTS = masq-up
pppdown_SCRIPTS = masq-down 
pkgconfig_DATA = affix.pc

EXTRA_DIST = affix.rc.deb affix.rc.rh $(affixconf_DATA) $(affixconf_SCRIPTS) \
	$(pppup_SCRIPTS) $(pppdown_SCRIPTS) $(pkgconfig_DATA)

install-exec-local:
	if [ -f /etc/debian_version ] ; then \
		cp affix.rc.deb $(DESTDIR)/etc/init.d/affix || true; \
		if [ "$(DESTDIR)" = "" ] ; then \
			update-rc.d affix defaults; \
		fi \
	fi
	if [ -f /etc/redhat-release ] ; then \
		cp affix.rc.rh $(DESTDIR)/etc/rc.d/init.d/affix || true; \
		if [ "$(DESTDIR)" = "" ] ; then \
			chkconfig --add affix; \
		fi \
	fi

uninstall-local:
	if [ -f /etc/debian_version ] ; then \
		if [ "$(DESTDIR)" = "" ] ; then \
			update-rc.d -f affix remove; \
		fi; \
		rm $(DESTDIR)/etc/init.d/affix; \
	fi
	if [ -f /etc/redhat-release ] ; then \
		if [ "$(DESTDIR)" = "" ] ; then \
			chkconfig --del affix; \
		fi; \
		rm $(DESTDIR)/etc/rc.d/init.d/affix; \
	fi

