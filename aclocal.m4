
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
