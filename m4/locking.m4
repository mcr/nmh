dnl Our functions to check the locking functions available and select a
dnl default locking function for the spool file
dnl
dnl Since we're assuming POSIX as a minimum, we always assume we have fcntl
dnl locking available.
dnl

AC_DEFUN([NMH_LOCKING],
[AC_CHECK_FUNCS([flock lockf])
AS_CASE(["$host_os"],
  [aix*|cygwin*|linux*],
    [default_locktype="fcntl"],
  [freebsd*|*netbsd*|openbsd*|darwin*], [default_locktype="flock"],
  [default_locktype="dot"])

AC_MSG_CHECKING([default locking method for the mail spool])

AC_ARG_WITH([locking],
  AS_HELP_STRING([--with-locking=@<:@dot|fcntl|flock|lockf@:>@],
    [The default locking method for the mail spool file]), ,
  [with_locking="$default_locktype"])

AS_CASE([$with_locking],
  [fcntl|dot], ,
  [flock],
    [AS_IF([test x"$ac_cv_func_flock" != x"yes"],
       [AC_MSG_ERROR([flock locks not supported on this system])])],
  [lockf],
    [AS_IF([test x"$ac_cv_func_lockf" != x"yes"],
       [AC_MSG_ERROR([lockf locks not supported on this system])])],
  [no],
    [AC_MSG_ERROR([--without-locking not supported])],
  [AC_MSG_ERROR([Unknown locking type $with_locking])])

AC_DEFINE_UNQUOTED([DEFAULT_LOCKING], ["$with_locking"],
  [The default lock type for the mail spool file])
AC_SUBST([default_locking], [$with_locking])

AC_MSG_RESULT([$with_locking])

supported_locks="fcntl dot"
AS_IF([test x"$ac_cv_func_flock" = x"yes"],
  [supported_locks="$supported_locks flock"])
AS_IF([test x"$ac_cv_func_lockf" = x"yes"],
  [supported_locks="$supported_locks lockf"])
AC_SUBST([supported_locks])

dnl Should we use a locking directory?
AC_ARG_ENABLE([lockdir],
  [AS_HELP_STRING([--enable-lockdir=dir], [Store dot-lock files in "dir"])], [
  AS_IF([test "x$enableval" = xyes],[
    AC_MSG_ERROR([--enable-lockdir requires an argument])])
  AC_DEFINE_UNQUOTED([LOCKDIR], ["$enableval"],
		     [Directory to store dot-locking lock files])
])])
