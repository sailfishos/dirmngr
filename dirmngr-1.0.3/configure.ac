# configure.ac - for DirMngr
#       Copyright (C) 2002 Klarälvdalens Datakonsult AB
#       Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008,
#                     2009 g10 Code GmbH
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
# USA.

# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)
min_automake_version="1.9.3"

# Remember to change the version number immediately *after* a release.
# Set my_issvn to "yes" for non-released code.  Remember to run an
# "svn up" and "autogen.sh" right before creating a distribution.
m4_define([my_version], [1.0.3])
m4_define([my_issvn], [no])

m4_define([svn_revision], m4_esyscmd([echo -n $( (svn info 2>/dev/null \
            || echo 'Revision: 0')|sed -n '/^Revision:/ {s/[^0-9]//gp;q;}')]))
AC_INIT([dirmngr],
        [my_version[]m4_if(my_issvn,[yes],[-svn[]svn_revision])],
        [bug-dirmngr@gnupg.org])

NEED_GPG_ERROR_VERSION=1.4

NEED_LIBGCRYPT_API=1
NEED_LIBGCRYPT_VERSION=1.4.0

NEED_LIBASSUAN_API=1
NEED_LIBASSUAN_VERSION=1.0.4

NEED_KSBA_API=1
NEED_KSBA_VERSION=1.0.2

COPYRIGHT_YEAR_NAME="2009 g10 Code GmbH"

PACKAGE=$PACKAGE_NAME
VERSION=$PACKAGE_VERSION


AC_CONFIG_SRCDIR(src/dirmngr.c)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE($PACKAGE, $VERSION)
AB_INIT

AC_GNU_SOURCE

# Some status variables to give feedback at the end of a configure run
have_gpg_error=no
have_libgcrypt=no 
have_libassuan=no
have_ksba=no
have_ldap=no
have_pth=no

AM_MAINTAINER_MODE

AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of this package])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Version of this package])
AC_DEFINE_UNQUOTED(PACKAGE_BUGREPORT, "$PACKAGE_BUGREPORT",
                                        [Bug report address])
AC_DEFINE_UNQUOTED(COPYRIGHT_YEAR_NAME, "$COPYRIGHT_YEAR_NAME",
                                        [Used by --version])
AC_DEFINE_UNQUOTED(NEED_LIBGCRYPT_VERSION, "$NEED_LIBGCRYPT_VERSION",
                                       [Required version of Libgcrypt])
AC_DEFINE_UNQUOTED(NEED_KSBA_VERSION, "$NEED_KSBA_VERSION",
                                       [Required version of Libksba])

# Checks for programs.
missing_dir=`cd $ac_aux_dir && pwd`
AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(AUTOMAKE, automake, $missing_dir)
AM_MISSING_PROG(AUTOHEADER, autoheader, $missing_dir)
AM_MISSING_PROG(MAKEINFO, makeinfo, $missing_dir)
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
#AC_ARG_PROGRAM
AC_SYS_LARGEFILE

AC_ARG_ENABLE(gcc-warnings,
	      AC_HELP_STRING([--enable-gcc-warnings],
			     [enable more verbose gcc warnings]),
	      [more_gcc_warnings="$enableval"],
	      [more_gcc_warnings="no"])

if test "$GCC" = yes; then
    if test "$USE_MAINTAINER_MODE" = "yes" ||
       test "$more_gcc_warnings" = "yes"; then
        CFLAGS="$CFLAGS -Wall -Wcast-align -Wshadow -Wstrict-prototypes"
        CFLAGS="$CFLAGS -Wformat -Wno-format-y2k -Wformat-security"
        if test "$more_gcc_warnings" = "yes"; then
            CFLAGS="$CFLAGS -W -Wpointer-arith -Wbad-function-cast"
            CFLAGS="$CFLAGS -Wwrite-strings"
            CFLAGS="$CFLAGS -Wno-sign-compare -Wno-missing-field-initializers"
        fi
    else
        CFLAGS="$CFLAGS -Wall"
    fi

    AC_MSG_CHECKING([if gcc supports -Wno-pointer-sign])
    _gcc_cflags_save=$CFLAGS
    CFLAGS="-Wno-pointer-sign"
    AC_COMPILE_IFELSE(AC_LANG_PROGRAM([]),_gcc_psign=yes,_gcc_psign=no)
    AC_MSG_RESULT($_gcc_psign)
    CFLAGS=$_gcc_cflags_save;
    if test x"$_gcc_psign" = xyes ; then
       CFLAGS="$CFLAGS -Wno-pointer-sign"
    fi
fi

# We need to compile and run a program on the build machine.  A
# comment in libgpg-error says that the AC_PROG_CC_FOR_BUILD macro in
# the AC archive is broken for autoconf 2.57.  Given that tehre is no
# newer version of that macro, we assume that it is also broken for
# autoconf 2.61 and thus we use a simple but usually sufficient
# approach.
AC_MSG_CHECKING(for cc for build)
if test "$cross_compiling" = "yes"; then
  CC_FOR_BUILD="${CC_FOR_BUILD-cc}"
else
  CC_FOR_BUILD="${CC_FOR_BUILD-$CC}"
fi
AC_MSG_RESULT($CC_FOR_BUILD)
AC_ARG_VAR(CC_FOR_BUILD,[build system C compiler])


# Tweaks for certain OSes.
mmap_needed=yes
have_w32_system=no
PRINTABLE_OS_NAME=
case "${host}" in
    *-*-mingw32*)
        PRINTABLE_OS_NAME="MingW32"
	mmap_needed=no
        have_w32_system=yes
        ;;
    *-*-cygwin*)
        PRINTABLE_OS_NAME="Cygwin"
        ;;
    i?86-emx-os2 | i?86-*-os2*emx )
        PRINTABLE_OS_NAME="OS/2"
        ;;
    i?86-*-msdosdjgpp*)
        PRINTABLE_OS_NAME="MSDOS/DJGPP"
        try_dynload=no
        ;;
    *-*-freebsd*)
       # Need to include /usr/local for FreeBSD
       CPPFLAGS="$CPPFLAGS -I/usr/local/include"
       LDFLAGS="$LDFLAGS -L/usr/local/lib"
       ;;
    *-linux*)
        PRINTABLE_OS_NAME="GNU/Linux"
        ;;
esac
if test -z "$PRINTABLE_OS_NAME"; then
  PRINTABLE_OS_NAME=`uname -s || echo "Unknown"`
fi

AC_DEFINE_UNQUOTED(PRINTABLE_OS_NAME, "$PRINTABLE_OS_NAME",
                   [A human readable text with the name of the OS])

if test "$have_w32_system" = yes; then
   AC_DEFINE(HAVE_W32_SYSTEM,1, [Defined if we run on a W32 API based system])
   AC_DEFINE(HAVE_DOSISH_SYSTEM,1, [Defined if we run on a DOS based system])
fi
AM_CONDITIONAL(HAVE_W32_SYSTEM, test "$have_w32_system" = yes)

AM_GNU_GETTEXT_VERSION([0.15])
AM_GNU_GETTEXT([external],[need-ngettext])

#
# Checks for libraries.
#
AM_ICONV

AC_REPLACE_FUNCS(vasprintf)
if test "$ac_cv_func_vasprintf" != yes; then
  GNUPG_CHECK_VA_COPY
fi

#
# We need the GNU Pth library
#
GNUPG_PATH_PTH
if test "$have_pth" = "yes"; then
  AC_DEFINE(USE_GNU_PTH, 1,
              [Defined if the GNU Portable Thread Library should be used])
fi


#
# libgpg-error is a library with error codes shared between GnuPG
# related projects.
#
AM_PATH_GPG_ERROR("$NEED_GPG_ERROR_VERSION",
                  have_gpg_error=yes,have_gpg_error=no)
AC_DEFINE(GPG_ERR_SOURCE_DEFAULT, GPG_ERR_SOURCE_DIRMNGR,
          [The default error source for GPGME.])

#
# Libgcrypt is our generic crypto library
#
AM_PATH_LIBGCRYPT("$NEED_LIBGCRYPT_API:$NEED_LIBGCRYPT_VERSION",
        have_libgcrypt=yes,have_libgcrypt=no)   


#
# libassuan is used for IPC
#
AM_PATH_LIBASSUAN("$NEED_LIBASSUAN_API:$NEED_LIBASSUAN_VERSION",
                  have_libassuan=yes,have_libassuan=no)
if test "$have_libassuan" = "yes"; then
  have_libassuan=no
  AM_PATH_LIBASSUAN_PTH("$NEED_LIBASSUAN_API:$NEED_LIBASSUAN_VERSION",
                        have_libassuan=yes,have_libassuan=no)
fi


#
# libksba is our X.509 support library
#
AM_PATH_KSBA("$NEED_KSBA_API:$NEED_KSBA_VERSION",have_ksba=yes,have_ksba=no)



#
# Must check for network library requirements before doing link tests
# for ldap, for example. If ldap libs are static (or dynamic and without
# ELF runtime link paths), then link will fail and LDAP support won't
# be detected.
#
AC_CHECK_FUNC(gethostbyname, , AC_CHECK_LIB(nsl, gethostbyname,
	[NETLIBS="-lnsl $NETLIBS"]))
AC_CHECK_FUNC(setsockopt, , AC_CHECK_LIB(socket, setsockopt,
	[NETLIBS="-lsocket $NETLIBS"]))

#
# LDAP libraries/includes
#
# From gnupg 1.4:
# Try and link a LDAP test program to weed out unusable LDAP
# libraries.  -lldap [-llber [-lresolv]] is for older OpenLDAPs.
# OpenLDAP, circa 1999, was terrible with creating weird dependencies.
# This seems to have all been resolved, so I'm simplifying this code
# significantly.  If all else fails, the user can play
# guess-the-dependency by using something like ./configure
# LDAPLIBS="-Lfoo -lbar"
#
for MY_LDAPLIBS in ${LDAPLIBS+"$LDAPLIBS"} "-lldap" "-lldap -llber" "-lldap -llber -lresolv" "-lwldap32"; do
    _ldap_save_libs=$LIBS
    LIBS="$MY_LDAPLIBS $NETLIBS $LIBS"

    AC_MSG_CHECKING([whether LDAP via \"$MY_LDAPLIBS\" is present and sane])
    AC_TRY_LINK([
#ifdef _WIN32
#include <winsock2.h>
#include <winldap.h>
#else
#include <ldap.h>
#endif
],[ldap_open("foobar",1234);],
                [gnupg_cv_func_ldap_init=yes],[gnupg_cv_func_ldap_init=no])
    AC_MSG_RESULT([$gnupg_cv_func_ldap_init])

    if test "$gnupg_cv_func_ldap_init" = yes ; then
       LDAPLIBS=$MY_LDAPLIBS
       have_ldap=yes

       AC_CHECK_FUNCS(ldap_get_option ldap_set_option)
       # The extra test for ldap_start_tls_sA is for W32 because 
       # that is the actual function in the library.
       AC_CHECK_FUNCS(ldap_start_tls_s ldap_start_tls_sA)

       if test "$ac_cv_func_ldap_get_option" != yes ; then
          AC_MSG_CHECKING([whether LDAP supports ld_errno])
	  AC_TRY_LINK([#include <ldap.h>],[LDAP *ldap; ldap->ld_errno;],
	     [gnupg_cv_func_ldap_ld_errno=yes],
	     [gnupg_cv_func_ldap_ld_errno=no])
          AC_MSG_RESULT([$gnupg_cv_func_ldap_ld_errno])

	  if test "$gnupg_cv_func_ldap_ld_errno" = yes ; then
	     AC_DEFINE(HAVE_LDAP_LD_ERRNO,1,
                      [Define if the LDAP library supports ld_errno])
          fi
       fi
    fi

    LIBS=$_ldap_save_libs

    if test $have_ldap = yes ; then break; fi
done
AC_SUBST(LDAPLIBS)



# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([string.h locale.h])


# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SIGNAL
AC_DECL_SYS_SIGLIST

GNUPG_CHECK_TYPEDEF(byte, HAVE_BYTE_TYPEDEF)
GNUPG_CHECK_TYPEDEF(ushort, HAVE_USHORT_TYPEDEF)
GNUPG_CHECK_TYPEDEF(ulong, HAVE_ULONG_TYPEDEF)


AC_CHECK_TYPES([struct sigaction, sigset_t],,,[#include <signal.h>])


# Checks for library functions.
GNUPG_FUNC_MKDIR_TAKES_ONE_ARG

# begin jnlib checks - fixme: we should have a macros for them
AC_CHECK_FUNCS([memicmp stpcpy strsep strlwr strtoul memmove stricmp strtol])
AC_CHECK_FUNCS([memrchr isascii timegm getrusage setrlimit stat setlocale])
AC_CHECK_FUNCS([flockfile funlockfile])
AC_CHECK_FUNCS([funopen fopencookie])
# end jnlib checks

AC_CHECK_FUNCS([gmtime_r])

AC_CHECK_FUNCS([getaddrinfo])

AC_CHECK_FUNCS([mmap])
if test $ac_cv_func_mmap != yes -a $mmap_needed = yes; then
  AC_MSG_ERROR([[Sorry, the current implemenation requires mmap.]])
fi

AC_CHECK_FUNCS([canonicalize_file_name])

#
# Stuff which goes at the bottom of config.h.
#
AH_BOTTOM([

/* Hack used for W32: ldap.m4 also tests for the ASCII version of
   ldap_start_tls_s because that is the actual symbol used in the
   library.  winldap.h redefines it to our commonly used value,
   thus we define our usual macro here.  */
#ifdef HAVE_LDAP_START_TLS_SA
# ifndef HAVE_LDAP_START_TLS_S
#  define HAVE_LDAP_START_TLS_S 1
# endif
#endif

/* We use jnlib, so tell other modules about it.  */
#define HAVE_JNLIB_LOGGING 1

/* We don't want the old assuan codes anymore. */
#define _ASSUAN_ONLY_GPG_ERRORS 1

/* Our HTTP code is used in estream mode.  */
#define HTTP_USE_ESTREAM 1

/* We want to use the libgcrypt provided memory allocation for
   asprintf.  */
#define _ESTREAM_PRINTF_MALLOC        gcry_malloc
#define _ESTREAM_PRINTF_FREE          gcry_free
#define _ESTREAM_PRINTF_EXTRA_INCLUDE "util.h"
])


#
# Print errors here so that they are visible all
# together and the user can acquire them all together.
#
die=no
if test "$have_gpg_error" = "no"; then
   die=yes
   AC_MSG_NOTICE([[
***  
*** You need libgpg-error to build this program.
**  This library is for example available at
***   ftp://ftp.gnupg.org/pub/gcrypt/libgpg-error
*** (at least version $NEED_GPG_ERROR_VERSION is required.)
***]])
fi
if test "$have_libgcrypt" = "no"; then
   die=yes
   AC_MSG_NOTICE([[
***  
*** You need libgcrypt to build this program.
**  This library is for example available at
***   ftp://ftp.gnupg.org/pub/gcrypt/libgcrypt/
*** (at least version $NEED_LIBGCRYPT_VERSION using API $NEED_LIBGCRYPT_API is required.)
***]])
fi
if test "$have_libassuan" = "no"; then
   die=yes
   AC_MSG_NOTICE([[
***
*** You need libassuan to build this program.
*** This library is for example available at
***   ftp://ftp.gnupg.org/pub/gcrypt/libassuan/
*** (at least version $NEED_LIBASSUAN_VERSION is required).
*** Note, that libassuan must have been build with Pth support.
***]])
fi
if test "$have_ksba" = "no"; then
   die=yes
    AC_MSG_NOTICE([[
***
*** You need libksba to build this program.
*** This library is for example available at
***   ftp://ftp.gnupg.org/pub/gcrypt/libksba/
*** (at least version $NEED_KSBA_VERSION using API $NEED_KSBA_API is required.)
***]])
fi
if test "$have_ldap" = "no"; then
   die=yes
    AC_MSG_NOTICE([[
***
*** You need a LDAP library to build this program.
*** Check out 
***    http://www.openldap.org 
*** for a suitable implementation. 
***]])
fi
if test "$have_pth" = "no"; then
   die=yes
    AC_MSG_NOTICE([[
***
*** GNU Portable Threads Library not found.  
*** Download GNU Pth from ftp://ftp.gnu.org/gnu/pth/
*** On a Debian GNU/Linux system you might want to try 
***   apt-get install libpth-dev
***]])
fi

if test "$die" = "yes"; then
    AC_MSG_ERROR([[
***
*** Required libraries not found. Please consult the above messages
*** and install them before running configure again.
***]])
fi



AC_CONFIG_FILES([ po/Makefile.in
Makefile
m4/Makefile
jnlib/Makefile
src/Makefile
doc/Makefile
doc/examples/Makefile
tests/Makefile
])
AC_OUTPUT
