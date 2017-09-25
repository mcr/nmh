dnl Updated for more modern systems.  Check to see if we need to link against
dnl optional libraries for networking functions.
dnl

AC_DEFUN([NMH_CHECK_NETLIBS],
[AC_SEARCH_LIBS([gethostbyname], [nsl], ,
		[AC_MSG_ERROR([gethostbyname not found])])
 AC_SEARCH_LIBS([connect], [socket], , [AC_MSG_ERROR([connect not found])])
])dnl
