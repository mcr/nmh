dnl Check for iconv support.  It may be in libiconv and may be iconv()
dnl or libiconv()
dnl

AC_DEFUN([NMH_CHECK_ICONV],
[ICONV_ENABLED=0
AC_CHECK_HEADER([iconv.h],
  [AC_CHECK_FUNC([iconv],
    [dnl This is where iconv is found in the default libraries (LIBS)
    nmh_found_iconv=yes
    dnl Handle the case where there is a native iconv but iconv.h is
    dnl from libiconv
    AC_CHECK_DECL([_libiconv_version],
      [AC_CHECK_LIB([iconv], [libiconv], [LIBS="-liconv $LIBS"])],,
      [#include <iconv.h>])],
    [dnl Since we didn't find iconv in LIBS, check libiconv
    AC_CHECK_LIB([iconv], [iconv], [nmh_found_iconv=yes],
      [dnl Also check for a function called libiconv()
      AC_CHECK_LIB([iconv], [libiconv], [nmh_found_iconv=yes])])
    dnl If either of these tests pass, set ICONVLIB
    AS_IF([test "x$nmh_found_iconv" = "xyes"], [ICONVLIB="-liconv"])])
  dnl If we came out of that by finding iconv in some form, define
  dnl HAVE_ICONV
  AS_IF([test "x$nmh_found_iconv" = "xyes"],
    [AC_DEFINE([HAVE_ICONV], [1],
		   [Define if you have the iconv() function.])
    ICONV_ENABLED=1
    AC_CACHE_CHECK([for iconv declaration], [nmh_cv_iconv_const],
      [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>
          #include <iconv.h>]],
	  [[#ifdef __cplusplus
	    "C"
	    #endif
	    #if defined(__STDC__) || defined(__cplusplus)
	    size_t iconv (iconv_t cd, char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);
	    #else
	    size_t iconv();
	    #endif]])],
        [nmh_cv_iconv_const=],
	[nmh_cv_iconv_const=const])])
    AC_DEFINE_UNQUOTED([ICONV_CONST], [$nmh_cv_iconv_const],
      [Define as const if the declaration of iconv() needs const.])])])
AC_SUBST([ICONVLIB])])
AC_SUBST([ICONV_ENABLED])
