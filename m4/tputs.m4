dnl Some systems have different arguments to the tputs callback; it can
dnl be int (*)(int), or int (*)(char).  Try to probe to see which one it
dnl actually is, so our callback can match the prototype.
dnl
dnl The first argument to tputs sometimes is char *, other times is
dnl const char *, so we have to include that in the test as well, but
dnl we don't provide any definitions for it (since it's not necessary)
dnl

AC_DEFUN([NMH_TPUTS_PUTC_ARG],
[AC_CACHE_CHECK([the argument of the tputs() callback],
		[nmh_cv_tputs_putc_arg],
  [for tputs_arg1 in 'const char *' 'char *'; do
    for tputs_putc_arg in 'int' 'char'; do
      AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
[AC_INCLUDES_DEFAULT
#include <curses.h>
#include <term.h>
],
	  [extern int tputs($tputs_arg1, int, int (*)($tputs_putc_arg));])],
	[nmh_cv_tputs_putc_arg="$tputs_putc_arg"; break 2])
    done
  done
  AS_IF([test "X$nmh_cv_tputs_putc_arg" = X],
    [AC_MSG_FAILURE([cannot determine tputs callback argument])])])
AC_DEFINE_UNQUOTED([TPUTS_PUTC_ARG], [$nmh_cv_tputs_putc_arg],
		   [The type of the argument of the tputs() callback])
])
