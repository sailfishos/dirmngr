# Copyright (C) 1999, 2000, 2001, 2004 Feee Software Soundation, Inc.
# 
# This file is part of GnuPG
# 
# GnuPG is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# GnuPG is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA

## Process this file with automake to produce Makefile.in

EXTRA_DIST = README

AM_CPPFLAGS = -I$(top_srcdir)/intl

# We need libgcrypt because libjnlib-config includes gcrypt.h
AM_CFLAGS = $(LIBGCRYPT_CFLAGS)

noinst_LIBRARIES = libjnlib.a


#libjnlib_a_LDFLAGS =
libjnlib_a_SOURCES = \
	libjnlib-config.h \
	stringhelp.c stringhelp.h \
	strlist.c strlist.h \
	utf8conv.c utf8conv.h \
	argparse.c argparse.h \
	logging.c logging.h  \
	dotlock.c dotlock.h  \
	types.h mischelp.h dynload.h w32help.h

if HAVE_W32_SYSTEM
libjnlib_a_SOURCES += w32-reg.c w32-afunix.c w32-afunix.h w32-gettext.c
endif
