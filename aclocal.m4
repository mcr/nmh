
# Originally by John Hawkinson <jhawk@mit.edu>
# Under Solaris, those
# applications need to link with "-lsocket -lnsl".  Under IRIX, they
# need to link with "-lnsl" but should *not* link with "-lsocket"
# because libsocket.a breaks a number of things (for instance,
# gethostbyname() under IRIX 5.2, and snoop sockets under most versions
# of IRIX).
#
# The check for libresolv is in case you are attempting to link
# statically and happen to have a libresolv.a lying around (and no
# libnsl.a). An example of such a case would be Solaris with
# BIND 4.9.5 installed.

AC_DEFUN(AC_CHECK_NETLIBS,
[AC_CHECK_FUNC(gethostbyname, ,
  AC_CHECK_LIB(nsl, gethostbyname, ,
    AC_CHECK_LIB(resolv, gethostbyname)))
AC_CHECK_FUNC(socket, ,
  AC_CHECK_LIB(socket, socket))
])

dnl --------------
dnl CHECK FOR NDBM
dnl --------------
dnl
dnl NMH_CHECK_DBM(libname,action-if-true,action-if-false,other-libraries)
dnl Check for presence of dbm_open() in the specified library
dnl (if libname is the empty string then don't try to link against
dnl any particular library). If action-if-true is unspecified it
dnl defaults to adding "-llibname" to the beginning of LIBS.
dnl If other-libraries is specified then these are prepended to
dnl LIBS for the duration of the check.
dnl NB that the checks for the right dbm header files must
dnl be done before using this macro!
dnl
dnl We try to link our own code fragment (which includes the
dnl headers in the same way slocal.c does) rather than
dnl using AC_CHECK_LIB because on later versions of libdb
dnl the dbm_open() function is provided via a #define and
dnl we don't want to hardcode searching for the internal
dnl function that lies behind it. (AC_CHECK_LIB works by
dnl defining its own bogus prototype rather than pulling in
dnl the right header files.)
AC_DEFUN(NMH_CHECK_DBM,
[
if test "x$1" == "x"; then
   nmh_libs=
   dnl this is just for the benefit of AC_CACHE_CHECK's message
   nmh_testname=libc
else
   nmh_libs="-l$1 "
   nmh_testname="$1"
fi
AC_CACHE_CHECK([for dbm in $nmh_testname], [nmh_cv_check_dbm_$1],[
nmh_saved_libs="$LIBS"
LIBS="$nmh_libs $4 $LIBS"
AC_LINK_IFELSE(AC_LANG_PROGRAM([[
#ifdef HAVE_DB1_NDBM_H
#include <db1/ndbm.h>
#else
#ifdef HAVE_GDBM_NDBM_H
#include <gdbm/ndbm.h>
#else
#if defined(HAVE_DB_H)
#define DB_DBM_HSEARCH 1
#include <db.h>
#else
#include <ndbm.h>
#endif
#endif
#endif
]],
[[dbm_open("",0,0);]]),[nmh_cv_check_dbm_$1=yes],[
nmh_cv_check_dbm_$1=no])
LIBS="$nmh_saved_libs"
])
if eval "test \"`echo '$nmh_cv_check_dbm_'$1`\" = yes"; then
  nmh_tr_macro=HAVE_LIB`echo $1 | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
  AC_DEFINE_UNQUOTED($nmh_tr_macro)
  m4_if([$2],,[LIBS="$nmh_libs$LIBS"],[$2])
else
  $3
  :
fi
])dnl
