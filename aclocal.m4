
#
# Updated for more modern systems.  Check to see if we need to link against
# optional libraries for networking functions.
#

AC_DEFUN([AC_CHECK_NETLIBS],
[AC_SEARCH_LIBS([gethostbyname], [nsl], ,
		[AC_MSG_ERROR([gethostbyname not found])])
 AC_SEARCH_LIBS([connect], [socket], , [AC_MSG_ERROR([connect not found])])
])dnl

dnl --------------
dnl CHECK FOR NDBM
dnl --------------
dnl
dnl NMH_CHECK_DBM(include,library,action-if-found,action-if-not)

dnl Check for presence of dbm_open() in the specified library
dnl and with the specified include file (if libname is the empty
dnl string then don't try to link against any particular library).

dnl We set nmh_ndbm_found to 'yes' or 'no'; if found we set
dnl nmh_ndbmheader to the first arg and nmh_ndbm to the second.

dnl If this macro accepted a list of include,library tuples
dnl to test in order that would be cleaner than the current
dnl nest of calls in configure.in.

dnl We try to link our own code fragment (which includes the
dnl headers in the same way slocal.c does) rather than
dnl using AC_CHECK_LIB because on later versions of libdb
dnl the dbm_open() function is provided via a #define and
dnl we don't want to hardcode searching for the internal
dnl function that lies behind it. (AC_CHECK_LIB works by
dnl defining its own bogus prototype rather than pulling in
dnl the right header files.)

dnl An oddity (bug) of this macro is that if you haven't
dnl done AC_PROG_CC or something that implies it before
dnl using this macro autoconf complains about a recursive
dnl expansion.

AC_DEFUN(NMH_CHECK_NDBM,
[
if test "x$2" = "x"; then
  nmh_libs=
  AC_MSG_CHECKING([for dbm in $1])
else
  nmh_libs="-l$2 "
  AC_MSG_CHECKING([for dbm in $1 and $2])
fi

dnl We don't try to cache the result, because that exceeds
dnl my autoconf skills -- feel free to put it in :-> -- PMM

nmh_saved_libs="$LIBS"
LIBS="$nmh_libs $5 $LIBS"
AC_LINK_IFELSE(AC_LANG_PROGRAM([[
#define DB_DBM_HSEARCH 1
#include <$1>
]],
[[dbm_open("",0,0);]]),[nmh_ndbm_found=yes],[nmh_ndbm_found=no])
LIBS="$nmh_saved_libs"

if test "$nmh_ndbm_found" = "yes"; then
  AC_MSG_RESULT(yes)
  nmh_ndbmheader="$1"
  nmh_ndbm="$2"
  $3
else
  AC_MSG_RESULT(no)
  $4
  :
fi
])dnl

dnl ----------------
dnl CHECK FOR d_type
dnl ----------------
dnl
dnl From Jim Meyering.
dnl
dnl Check whether struct dirent has a member named d_type.
dnl

# Copyright (C) 1997, 1999, 2000, 2001, 2002, 2003, 2004 Free Software
# Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

AC_DEFUN([CHECK_TYPE_STRUCT_DIRENT_D_TYPE],
  [AC_REQUIRE([AC_HEADER_DIRENT])dnl
   AC_CACHE_CHECK([for d_type member in directory struct],
                  jm_cv_struct_dirent_d_type,
     [AC_TRY_LINK(dnl
       [
#include <sys/types.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>
#else /* not HAVE_DIRENT_H */
# define dirent direct
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif /* HAVE_SYS_NDIR_H */
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif /* HAVE_SYS_DIR_H */
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif /* HAVE_NDIR_H */
#endif /* HAVE_DIRENT_H */
       ],
       [struct dirent dp; dp.d_type = 0;],

       jm_cv_struct_dirent_d_type=yes,
       jm_cv_struct_dirent_d_type=no)
     ]
   )
   if test $jm_cv_struct_dirent_d_type = yes; then
     AC_DEFINE(HAVE_STRUCT_DIRENT_D_TYPE, 1,
       [Define if there is a member named d_type in the struct describing
        directory headers.])
   fi
  ]
)
