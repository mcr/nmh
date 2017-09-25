dnl Our readline heuristic.  If we haven't been asked about readline, then
dnl try to compile with it.  If we've been asked for it, then we fail
dnl if we cannot use it.  If we were explicitly NOT asked for it, then
dnl don't even try to use it.
dnl

AC_DEFUN([NMH_READLINE],
[AC_ARG_WITH([readline],
	AS_HELP_STRING([--with-readline],
		       [enable readline editing for whatnow (default=maybe)]),
	[], [with_readline=maybe])
AS_IF([test x"$with_readline" = xyes -o x"$with_readline" = xmaybe],
    [save_LIBS="$LIBS"
    LIBS=
    AC_SEARCH_LIBS([readline], [readline editline],
		   [READLINELIB="$LIBS"
		   AC_DEFINE([READLINE_SUPPORT], [1],
			     [Support for using readline() in whatnow])],
		   [AS_IF([test x"$with_readline" = xyes],
			  [AC_MSG_ERROR([Unable to find a readline library])])],
		   [$TERMLIB])
    LIBS="$save_LIBS"])
])

AC_SUBST([READLINELIB])
