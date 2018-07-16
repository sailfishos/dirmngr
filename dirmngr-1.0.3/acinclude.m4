dnl macros to configure gnupg
dnl Copyright (C) 1998, 1999, 2000, 2001 Free Software Foundation, Inc.
dnl
dnl This file is part of GnuPG.
dnl
dnl GnuPG is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl 
dnl GnuPG is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl 
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA

dnl GNUPG_CHECK_TYPEDEF(TYPE, HAVE_NAME)
dnl Check whether a typedef exists and create a #define $2 if it exists
dnl
AC_DEFUN([GNUPG_CHECK_TYPEDEF],
  [ AC_MSG_CHECKING(for $1 typedef)
    AC_CACHE_VAL(gnupg_cv_typedef_$1,
    [AC_TRY_COMPILE([#define _GNU_SOURCE 1
    #include <stdlib.h>
    #include <sys/types.h>], [
    #undef $1
    int a = sizeof($1);
    ], gnupg_cv_typedef_$1=yes, gnupg_cv_typedef_$1=no )])
    AC_MSG_RESULT($gnupg_cv_typedef_$1)
    if test "$gnupg_cv_typedef_$1" = yes; then
        AC_DEFINE($2,1,[Defined if a `]$1[' is typedef'd])
    fi
  ])

dnl Stolen from gcc
dnl Define MKDIR_TAKES_ONE_ARG if mkdir accepts only one argument instead
dnl of the usual 2.
AC_DEFUN([GNUPG_FUNC_MKDIR_TAKES_ONE_ARG],
[AC_CHECK_HEADERS(sys/stat.h unistd.h direct.h)
AC_CACHE_CHECK([if mkdir takes one argument], gnupg_cv_mkdir_takes_one_arg,
[AC_TRY_COMPILE([
#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_DIRECT_H
# include <direct.h>
#endif], [mkdir ("foo", 0);],
        gnupg_cv_mkdir_takes_one_arg=no, gnupg_cv_mkdir_takes_one_arg=yes)])
if test $gnupg_cv_mkdir_takes_one_arg = yes ; then
  AC_DEFINE(MKDIR_TAKES_ONE_ARG,1,
            [Defined if mkdir() does not take permission flags])
fi
])


dnl GNUPG_CHECK_VA_COPY()
dnl   Do some check on how to implement va_copy.
dnl   May define MUST_COPY_VA_BY_VAL.
dnl   Actual test code taken from glib-1.1.
AC_DEFUN([GNUPG_CHECK_VA_COPY],
[ AC_MSG_CHECKING(whether va_lists must be copied by value)
  AC_CACHE_VAL(gnupg_cv_must_copy_va_byval,[
    if test "$cross_compiling" = yes; then
      gnupg_cv_must_copy_va_byval=no
    else
      gnupg_cv_must_copy_va_byval=no
      AC_TRY_RUN([
       #include <stdarg.h>
       void f (int i, ...)
       {
          va_list args1, args2;
          va_start (args1, i);
          args2 = args1;
          if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
            exit (1);
          va_end (args1);
          va_end (args2);
       }
      
       int main()
       {
          f (0, 42);
            return 0;
       }
      ],gnupg_cv_must_copy_va_byval=yes)
    fi
  ])
  if test "$gnupg_cv_must_copy_va_byval" = yes; then
     AC_DEFINE(MUST_COPY_VA_BYVAL,1,[used to implement the va_copy macro])
  fi
  if test "$cross_compiling" = yes; then
    AC_MSG_RESULT(assuming $gnupg_cv_must_copy_va_byval)
  else
    AC_MSG_RESULT($gnupg_cv_must_copy_va_byval)
  fi
])
