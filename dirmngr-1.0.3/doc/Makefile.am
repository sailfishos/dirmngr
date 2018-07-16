# Makefile.am - main makefile for Dirmngr
#     	Copyright (C) 2002 Klarälvdalens Datakonsult AB
# 
# This file is part of Dirmngr.
# 
# Dirmngr is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# Dirmngr is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA

## Process this file with automake to produce Makefile.in

EXTRA_DIST = internals.texi see-also-note.texi  yat2m.c

SUBDIRS = . examples

AM_MAKEFINFOFLAGS = -I $(srcdir) --css-include=/dev/null

YAT2M_OPTIONS = -I $(srcdir) \
        --release "Dirmngr @PACKAGE_VERSION@" --source "GNU Privacy Guard"

myman_sources = dirmngr.texi
myman_pages   = dirmngr.1 dirmngr-client.1

man_MANS = $(myman_pages)

info_TEXINFOS = dirmngr.texi

dirmngr_TEXINFOS = gpl.texi

CLEANFILES = yat2m

DISTCLEANFILES = dirmngr.ops yat2m-stamp.tmp yat2m-stamp $(myman_pages)

yat2m: yat2m.c
	$(CC_FOR_BUILD) -o $@ $(srcdir)/yat2m.c


yat2m-stamp: $(myman_sources)
	@rm -f yat2m-stamp.tmp
	@touch yat2m-stamp.tmp
	for file in $(myman_sources) ; do \
              ./yat2m $(YAT2M_OPTIONS) --store \
	          `test -f '$$file' || echo '$(srcdir)/'`$$file ; done
	@mv -f yat2m-stamp.tmp $@

yat2m-stamp: yat2m

$(myman_pages) : yat2m-stamp
	@if test -f $@; then :; else \
            trap 'rm -rf yat2m-stamp yat2m-lock' 1 2 13 15; \
               if mkdir yat2m-lock 2>/dev/null; then \
                 rm -f yat2m-stamp; \
                 $(MAKE) $(AM_MAKEFLAGS) yat2m-stamp; \
                 rmdir yat2m-lock; \
               else \
                 while test -d yat2m-lock; do sleep 1; done; \
                 test -f yat2m-stamp; exit $$?; \
               fi; \
             fi


online: dirmngr.html dirmngr.pdf
	set -e; \
	echo "Uploading current manuals to www.gnupg.org ..."; \
        user=werner ; \
	(cd dirmngr.html && rsync -vr --exclude='.svn' .  \
	  $${user}@cvs.gnupg.org:webspace/manuals/dirmngr/ ); \
        rsync -v dirmngr.pdf $${user}@cvs.gnupg.org:webspace/manuals/

