
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


# This checks for the function ruserpass.
#
# 1) first, check for ruserpass
# 2) else, check for _ruserpass
# 3) else, check for _ruserpass in libsocket
# 4) else, build version of ruserpass in nmh/sbr
AC_DEFUN(AC_CHECK_RUSERPASS,
[AC_CHECK_FUNC(ruserpass, ,
  AC_CHECK_FUNC(_ruserpass, ,
    AC_CHECK_LIB(socket, _ruserpass)))
if test x$ac_cv_func_ruserpass = xno; then
  if test x$ac_cv_func__ruserpass = xyes -o x$ac_cv_lib_socket__ruserpass = xyes; then
    AC_DEFINE(ruserpass, _ruserpass)
  else
    LIBOBJS="$LIBOBJS ruserpass.o"
  fi
fi
])
