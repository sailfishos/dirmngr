# Makefile.am - main makefile for DirMngr
#     	Copyright (C) 2002 Klarälvdalens Datakonsult AB
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

ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = dist-bzip2 no-dist-gzip

EXTRA_DIST = config.rpath autogen.sh

if HAVE_W32_SYSTEM
tests =
else
tests = tests
endif

SUBDIRS = m4 jnlib src po doc $(tests)

dist-hook:
	echo "$(VERSION)" > $(distdir)/VERSION

stowinstall: 
	$(MAKE) $(AM_MAKEFLAGS) install prefix=/usr/local/stow/dirmngr
