
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
