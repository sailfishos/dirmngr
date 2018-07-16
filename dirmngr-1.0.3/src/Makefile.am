# Makefile.am - dirmngr
# Copyright (C) 2002 Klarälvdalens Datakonsult AB
# Copyright (C) 2004, 2007 g10 Code GmbH
#
# This file is part of DirMngr.
#
# DirMngr is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# DirMngr is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA

## Process this file with automake to produce Makefile.in

localedir = $(datadir)/locale

EXTRA_DIST = Manifest

bin_PROGRAMS = dirmngr dirmngr-client

libexec_PROGRAMS = dirmngr_ldap

AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\" 
AM_CPPFLAGS += -DDIRMNGR_SYSCONFDIR="\"$(sysconfdir)/@PACKAGE@\"" 
AM_CPPFLAGS += -DDIRMNGR_LIBEXECDIR="\"$(libexecdir)\""
AM_CPPFLAGS += -DDIRMNGR_DATADIR="\"$(localstatedir)/lib/@PACKAGE@\""
AM_CPPFLAGS += -DDIRMNGR_CACHEDIR="\"$(localstatedir)/cache/@PACKAGE@\""
AM_CPPFLAGS += -DDIRMNGR_SOCKETDIR="\"$(localstatedir)/run/@PACKAGE@\""

AM_CFLAGS = -I$(top_srcdir)/jnlib $(LIBGCRYPT_CFLAGS) $(LIBASSUAN_CFLAGS) \
              $(KSBA_CFLAGS) $(GPG_ERROR_CFLAGS) $(PTH_CFLAGS)

noinst_HEADERS = dirmngr.h crlcache.h crlfetch.h error.h misc.h

dirmngr_SOURCES = dirmngr.c dirmngr.h server.c crlcache.c crlfetch.c	\
	ldapserver.h ldapserver.c certcache.c certcache.h i18n.h	\
	util.h b64dec.c cdb.h cdblib.c ldap.c http.c http.h misc.c	\
	ocsp.c ocsp.h estream.c estream.h estream-printf.c		\
	estream-printf.h validate.c validate.h exechelp.h exechelp.c	\
	get-path.c

dirmngr_LDADD = ../jnlib/libjnlib.a $(LIBOBJS) $(LIBASSUAN_PTH_LIBS)  \
	$(LIBGCRYPT_LIBS) $(KSBA_LIBS) $(PTH_LIBS) $(LIBINTL) $(LIBICONV)

if HAVE_W32_SYSTEM
ldap_url = ldap-url.h ldap-url.c
else
ldap_url =
endif

dirmngr_ldap_SOURCES = dirmngr_ldap.c i18n.h util.h \
                       no-libgcrypt.c no-libgcrypt.h $(ldap_url)
dirmngr_ldap_CFLAGS = $(GPG_ERROR_CFLAGS)
dirmngr_ldap_LDFLAGS =
dirmngr_ldap_LDADD = $(ldap_ldadd) ../jnlib/libjnlib.a $(LIBOBJS) \
		     $(GPG_ERROR_LIBS) $(LDAPLIBS) $(LIBINTL) $(LIBICONV)

dirmngr_client_SOURCES = dirmngr-client.c i18n.h util.h b64enc.c \
                         get-path.c no-libgcrypt.c no-libgcrypt.h 
dirmngr_client_LDADD = ../jnlib/libjnlib.a $(LIBOBJS) $(LIBASSUAN_LIBS) \
	               $(GPG_ERROR_LIBS) $(LIBINTL) $(LIBICONV)

