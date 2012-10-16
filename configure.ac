dnl
dnl configure.ac -- autoconf template for nmh
dnl

dnl Move this up a bit
AC_PREREQ([2.61])

AC_INIT([nmh], m4_normalize(m4_include([VERSION])), [nmh-workers@nongnu.org])
AC_CONFIG_SRCDIR([h/nmh.h])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])

AC_CANONICAL_HOST

AC_MSG_NOTICE([configuring for AC_PACKAGE_NAME-AC_PACKAGE_VERSION])
AC_SUBST(VERSION,AC_PACKAGE_VERSION)dnl

dnl What date of nmh are we building?
DATE=`cat ${srcdir}/DATE`
AC_MSG_NOTICE([configuring for nmh dated $DATE])
AC_SUBST([DATE])dnl

dnl --------------------------
dnl CHECK COMMAND LINE OPTIONS
dnl --------------------------

dnl Do you want to debug nmh?
AC_ARG_ENABLE([debug],
  AS_HELP_STRING([--enable-debug],[enable nmh code debugging]))
dnl The old redundant --enable-nmh-debug is deprecated and undocumented.
AS_IF([test x"$enable_nmh_debug" = x"yes"], [enable_debug=yes])

dnl Do you want to disable use of locale functions
AH_TEMPLATE([LOCALE],
[Undefine if you don't want locale features.  By default this is defined.])
AC_ARG_ENABLE([locale],
  [AS_HELP_STRING([--disable-locale], [turn off locale features])],
  [AS_IF([test x$enableval = xyes], [AC_DEFINE(LOCALE)])],
  [AC_DEFINE(LOCALE)])

dnl Do you want client-side support for using SASL for authentication?
dnl Note that this code will be enabled for both POP and SMTP
AC_ARG_WITH([cyrus-sasl], AS_HELP_STRING([--with-cyrus-sasl],
  [Enable SASL support via the Cyrus SASL library]))
AS_IF([test x"$with_cyrus_sasl" != x -a x"$with_cyrus_sasl" != x"no"],[
      AC_DEFINE([CYRUS_SASL], [1],
		[Define to use the Cyrus SASL library for authentication of POP and SMTP.])dnl
      AS_IF([test x"$with_cyrus_sasl" != xyes],[
	    AC_MSG_WARN([Arguments to --with-cyrus-sasl now ignored])
	    AC_MSG_WARN([Please pass the appropriate arguments to CPPFLAGS/LDFLAGS])])
      sasl_support=yes], [sasl_support=no])

dnl Do you want client-side support for encryption with TLS?
AC_ARG_WITH([tls], AS_HELP_STRING([--with-tls], [Enable TLS support]))
AS_IF([test x"$with_tls" != x -a x"$with_tls" != x"no"],[
      AC_DEFINE([TLS_SUPPORT], [1], [Support TLS for session encryption.])dnl
      tls_support=yes],[tls_support=no])

dnl What should be the default editor?
AC_ARG_WITH([editor],
  AS_HELP_STRING([--with-editor=EDITOR],[specify the default editor]))

AS_IF([test -n "$with_editor"], [editorpath="$with_editor"])

dnl Set the backup prefix
AC_ARG_WITH([hash-backup],
  AS_HELP_STRING([--with-hash-backup],[use # as the backup prefix (default: ,)]))
AS_IF([test x"$with_hash_backup" != x -a x"$with_hash_backup" != x"no"],
      [backup_prefix="#"], [backup_prefix=","])
AC_DEFINE_UNQUOTED([BACKUP_PREFIX], "$backup_prefix",
    [The prefix that is prepended to the name of message files when they are "removed" by rmm. This should typically be `,' or `#'.])dnl

dnl What method of locking to use?
AS_CASE(["$host_os"],
  [aix*|cygwin*|linux*],
    [default_locktype="fcntl"; default_locking=FCNTL_LOCKING],
  [freebsd*|openbsd*|darwin*], [default_locktype="flock"; default_locking=FLOCK_LOCKING],
  [default_locktype="dot"; default_locking=DOT_LOCKING])

AC_ARG_WITH(locking,
  AS_HELP_STRING([--with-locking=@<:@dot|fcntl|flock|lockf@:>@],
  [specify the file locking method]))

if test x"$with_locking" = x"dot"; then
  LOCKTYPE="dot"
  AC_DEFINE(DOT_LOCKING, 1, [Define to use dot based file locking.])dnl
elif test x"$with_locking" = x"flock"; then
  LOCKTYPE="flock"
  AC_DEFINE(FLOCK_LOCKING, 1, [Define to use flock() based locking.])dnl
elif test x"$with_locking" = x"lockf"; then
  LOCKTYPE="lockf"
  AC_DEFINE(LOCKF_LOCKING, 1, [Define to use lockf() based locking.])dnl
elif test x"$with_locking" = x"fcntl"; then
  LOCKTYPE="fcntl"
  AC_DEFINE(FCNTL_LOCKING, 1, [Define to use fnctl() based locking.])dnl
else
  LOCKTYPE="$default_locktype"
  AC_DEFINE_UNQUOTED($default_locking, 1)dnl
fi

dnl Should we use a locking directory?
AC_ARG_ENABLE([lockdir],
  [AS_HELP_STRING([--enable-lockdir=dir], [Store dot-lock files in "dir"])], [
  AS_IF([test "x$enableval" = xyes],[
    AC_MSG_ERROR([--enable-lockdir requires an argument])])
  AS_IF([test "x$LOCKTYPE" != xdot],[
    AC_MSG_ERROR([Can only use --enable-lockdir with dot locking])])
  AC_DEFINE_UNQUOTED([LOCKDIR], ["$enableval"],
  		     [Directory to store dot-locking lock files])
])

dnl What method of posting should post use?
AC_ARG_WITH([mts],
  AS_HELP_STRING([--with-mts=@<:@smtp|sendmail/smtp|sendmail/pipe@:>@],
  [specify the default mail transport agent/service]))

AS_IF([test x"$with_mts" = x"smtp"], [MTS="smtp"],
      [test x"$with_mts" = x"sendmail"], [MTS="sendmail/smtp"],
      [test x"$with_mts" = x"sendmail/smtp"], [MTS="sendmail/smtp"],
      [test x"$with_mts" = x"sendmail/pipe"], [MTS="sendmail/pipe"],
      [MTS="smtp"])
AC_SUBST([MTS])dnl

dnl What should be the default pager?
AC_ARG_WITH([pager],
  AS_HELP_STRING([--with-pager=PAGER],[specify the default pager]))

AS_IF([test -n "$with_pager"], [pagerpath="$with_pager"])

dnl If a pager is not explicitly specified, then look for one.
dnl Fall back to `cat', to avoid catastrophic failure of
dnl `mhshow', et al., if pagerpath is set to `no'.
if test -z "$pagerpath"; then
  pathtmp=/usr/bin:/bin:/usr/ucb:/usr/local/bin
  AC_PATH_PROGS([pagerpath], [more less most cat], [no], [$pathtmp])
fi
AC_SUBST(pagerpath)dnl

dnl What should be the default mail server(s)?
AC_ARG_WITH(smtpservers,
  AS_HELP_STRING([--with-smtpservers='SMTPSERVER1@<:@ SMTPSERVER2...@:>@'],
  [specify the default SMTP server(s) @<:@localhost@:>@]))
AS_IF([test -n "$with_smtpservers"], [smtpservers="$with_smtpservers"],
      [smtpservers="localhost"])
AC_SUBST([smtpservers])dnl

dnl ----------------------------------------------------
dnl Default location is /usr/local/nmh/{bin,etc,lib,man}
dnl ----------------------------------------------------
AC_PREFIX_DEFAULT([/usr/local/nmh])

dnl ------------------
dnl CHECK THE COMPILER
dnl ------------------
dnl We want these before the checks,
dnl so the checks can modify their values.
test -z "$CFLAGS" && CFLAGS= auto_cflags=1
if test x"$enable_debug" = x"yes"; then
  test -z "$LDFLAGS" && LDFLAGS=-g
fi

AC_PROG_CC
AM_PROG_CC_C_O

AC_CACHE_CHECK([whether preprocessor supports -Wunused-macros],
  [nmh_cv_has_unusedmacros],
  [nmh_saved_cppflags="$CPPFLAGS"
   CPPFLAGS="$CPPFLAGS -Wunused-macros -Wno-unused-macros"
   AC_TRY_COMPILE([],[],nmh_cv_has_unusedmacros=yes,nmh_cv_has_unusedmacros=no)
   CPPFLAGS="$nmh_saved_cppflags"])

if test "$nmh_cv_has_unusedmacros" = 'yes'; then
  test -z "$CPPFLAGS"  &&  CPPFLAGS=-Wunused-macros \
                      ||  CPPFLAGS="$CPPFLAGS -Wunused-macros"
  DISABLE_UNUSED_MACROS_WARNING=-Wno-unused-macros
fi
AC_SUBST(DISABLE_UNUSED_MACROS_WARNING)dnl

AC_CACHE_CHECK([whether compiler supports -Wno-sign-compare],
  [nmh_cv_has_nosigncompare],
  [nmh_saved_cppflags="$CPPFLAGS"
   CPPFLAGS="$CPPFLAGS -Wno-sign-compare"
   AC_TRY_COMPILE([],[],nmh_cv_has_nosigncompare=yes,
                  nmh_cv_has_nosigncompare=no)
   CPPFLAGS="$nmh_saved_cppflags"])

if test "$nmh_cv_has_nosigncompare" = 'yes'; then
  DISABLE_SIGN_COMPARE_WARNING=-Wno-sign-compare
fi
AC_SUBST(DISABLE_SIGN_COMPARE_WARNING)dnl

AC_CACHE_CHECK([whether compiler supports -Wextra], [nmh_cv_has_wextra],
[nmh_saved_cflags="$CFLAGS"
 CFLAGS="$CFLAGS -Wextra -Wno-clobbered"
 AC_TRY_COMPILE([],[],nmh_cv_has_wextra=yes,nmh_cv_has_wextra=no)
 CFLAGS="$nmh_saved_cflags"])

AC_CACHE_CHECK([whether compiler supports -Wno-pointer-sign], [nmh_cv_has_noptrsign],
[nmh_saved_cflags="$CFLAGS"
 CFLAGS="$CFLAGS -Wno-pointer-sign"
 AC_TRY_COMPILE([],[],nmh_cv_has_noptrsign=yes,nmh_cv_has_noptrsign=no)
 CFLAGS="$nmh_saved_cflags"])

dnl Can't use -ansi with gcc 4.5.3 on Cygwin, at least through setup
dnl setup version 2.763, because it disables some features in the
dnl system system header files and warns about them with -Wall.  Try
dnl to test for that generally, though still with gcc.
if test "$GCC" = yes; then
  AC_CACHE_CHECK([whether we can use -ansi with gcc], [nmh_cv_has_dash_ansi],
    [nmh_saved_cppflags="$CPPFLAGS"
     nmh_saved_cflags="$CFLAGS"
     CPPFLAGS="-D_GNU_SOURCE"
     CFLAGS="-ansi -Wall -Werror"
     AC_COMPILE_IFELSE([AC_LANG_PROGRAM([#include <time.h>],[tzset();])],
       [nmh_cv_has_dash_ansi=yes],
       [nmh_cv_has_dash_ansi=no])
     CPPFLAGS="$nmh_saved_cppflags"
     CFLAGS="$nmh_saved_cflags"])
else
  nmh_cv_has_dash_ansi=no
fi

dnl if the user hasn't specified CFLAGS, then
dnl   if compiler is gcc, then
dnl    use -O2 and some warning flags
dnl   else use -O
dnl We use -Wall and -Wextra if supported.  If the compiler supports it we
dnl also use -Wno-pointer-sign, because gcc 4 now produces a lot of new
dnl warnings which are probably mostly spurious and which in any case we
dnl don't want to deal with now.
if test "$nmh_cv_has_noptrsign" = "yes"; then
    if test "$nmh_cv_has_dash_ansi" = "yes"; then
        nmh_gcc_common_flags="-ansi -pedantic -Wall"
    else
        nmh_gcc_common_flags="-Wall"
    fi
    if test "$nmh_cv_has_wextra" = "yes"; then
        nmh_gcc_warnflags="${nmh_gcc_common_flags} -Wextra -Wno-clobbered "\
"-Wno-pointer-sign"
    else
        nmh_gcc_warnflags="${nmh_gcc_common_flags} -Wno-pointer-sign"
    fi
else
    if test "$nmh_cv_has_wextra" = "yes"; then
        nmh_gcc_warnflags="${nmh_gcc_common_flags} -Wextra -Wno-clobbered"
    else
        nmh_gcc_warnflags="${nmh_gcc_common_flags}"
    fi
fi

if test -n "$auto_cflags"; then
  if test x"$enable_debug" = x"yes"; then
    if test -n "$GCC"; then
      test -z "$CFLAGS" && CFLAGS="$nmh_gcc_warnflags -g" || CFLAGS="$CFLAGS $nmh_gcc_warnflags -g"
    else
      test -z "$CFLAGS" && CFLAGS=-g || CFLAGS="$CFLAGS -g"
    fi
  else
    if test -z "$LDFLAGS"; then
      case "$build_os" in
	darwin*)
 	  LDFLAGS=
          ;;
	*)
	  LDFLAGS=-s
	  ;;
      esac
    fi
    if test -n "$GCC"; then
      test -z "$CFLAGS" && CFLAGS="$nmh_gcc_warnflags -O2" || CFLAGS="$CFLAGS $nmh_gcc_warnflags -O2"
    else
      test -z "$CFLAGS" && CFLAGS=-O  || CFLAGS="$CFLAGS -O"
    fi
  fi
fi

AC_C_CONST              dnl Does compiler support `const'.

dnl ------------------
dnl CHECK FOR PROGRAMS
dnl ------------------
AC_PROG_MAKE_SET	dnl Does make define $MAKE
AC_PROG_INSTALL		dnl Check for BSD compatible `install'
AC_PROG_RANLIB		dnl Check for `ranlib'
AC_PROG_AWK             dnl Check for mawk,gawk,nawk, then awk
AC_PROG_SED		dnl Check for Posix-compliant sed
AC_PROG_LEX             dnl Check for lex/flex

dnl Look for `cut'
pathtmp=/usr/bin:/bin:/usr/local/bin:/usr/xpg4/bin:/usr/ucb
AC_PATH_PROG([cutpath], [cut], [no], [$pathtmp])

dnl
dnl Check for MD5 program and formatting command
dnl
AC_CHECK_PROGS([MD5SUM], [md5sum md5], [missing])
AS_CASE(["${MD5SUM}"],
	[md5sum], [MD5FMT="cat"],
	[md5], [[MD5FMT="${SED} -e 's/MD5 *(\(.*\)) *= \([0-9a-f]*\)/\2  \1/'"]],
	[MD5FMT="missing"])
AC_SUBST([MD5FMT])

dnl ----------------------------------------------
dnl check for lclint, and lint if it doesn't exist
dnl ----------------------------------------------
AC_CHECK_PROG(linttmp1, lclint, lclint, no)dnl
if test x$ac_cv_prog_linttmp1 != xno ; then
  LINT=$ac_cv_prog_linttmp1
  LINTFLAGS="-weak +posixlib -macrovarprefixexclude"
else
  AC_CHECK_PROG(linttmp2, lint, lint, no)dnl
  if test x$ac_cv_prog_linttmp2 != xno ; then
    LINT=$ac_cv_prog_linttmp2
    LINTFLAGS=""
  else
    LINT="echo 'No lint program found'"
    LINTFLAGS=""
  fi
fi
AC_SUBST([LINT])dnl
AC_SUBST([LINTFLAGS])dnl

dnl Look for `ls'
pathtmp=/usr/bin:/bin:/usr/local/bin:/usr/xpg4/bin:/usr/ucb
AC_PATH_PROG([lspath], [ls], [no], [$pathtmp])

dnl See how we get ls to display the owner and the group
if test "$lspath" != "no"; then
  AC_CACHE_CHECK([how to get ls to show us the group ownership of a file],
		 [nmh_cv_ls_grpopt],
  [if test x"`$lspath -dl / | $AWK '{print $9}'`" = x"/"; then
    dnl There were 9 parameters, so unless this is a really bizarre, nonstandard
    dnl ls, it would seem -l gave us both the user and group.  On this type of
    dnl ls, -g makes _only_ the group be displayed (and not the user).
    nmh_cv_ls_grpopt="-l"
  else
    dnl Looks like -l only gave us the user, so we need -g to get the group too.
    nmh_cv_ls_grpopt="-lg"
  fi])
fi

dnl Look for `sendmail'
pathtmp=/usr/lib:/usr/sbin:/usr/etc:/usr/ucblib:/usr/bin:/bin
AC_PATH_PROG([sendmailpath], [sendmail], [/usr/sbin/sendmail], [$pathtmp])

dnl Look for `vi'
pathtmp=/usr/bin:/bin:/usr/ucb:/usr/local/bin
AC_PATH_PROG([vipath], [vi], [/bin/vi], [$pathtmp])

dnl If editor is not specified yet,
dnl then use `vi' as the default.
AS_IF([test -z "$editorpath"], [editorpath="$vipath"])
AC_SUBST([editorpath])dnl

dnl Cygwin FAT filesystems do not support hard links.  So default to
dnl cp instead, even if running on an NTFS or other filesystem.
AS_CASE(["$host_os"],
  [cygwin*],[LN=cp],
  [LN=ln])
AC_SUBST([LN])

dnl ----------------------------------------------------------
dnl FIND MAIL SPOOL AND SEE IF WE NEED TO MAKE inc SETGID MAIL
dnl ----------------------------------------------------------
AC_CACHE_CHECK(where mail spool is located, nmh_cv_mailspool,
[for mailspool in /var/mail        dnl
                  /var/spool/mail  dnl
                  /usr/spool/mail  dnl
                  /dev/null;       dnl Just in case we fall through
do
  test -d $mailspool && break
done
nmh_cv_mailspool=$mailspool
])
mailspool=$nmh_cv_mailspool
AC_SUBST([mailspool])dnl

dnl See whether the mail spool directory is world-writable.
if test "$lspath" != "no" -a "$cutpath" != "no"; then
  AC_CACHE_CHECK(whether the mail spool is world-writable,
		 nmh_cv_mailspool_world_writable,
  [if test "`$lspath -dlL $mailspool | $cutpath -c9`" = "-"; then
    nmh_cv_mailspool_world_writable=no
  else
    nmh_cv_mailspool_world_writable=yes
  fi])
fi

dnl Also, check for liblockfile (as found on Debian systems)
AC_CHECK_HEADER([lockfile.h], [AC_CHECK_LIB(lockfile, lockfile_create)])

dnl and whether its companion program dotlockfile is setgid
AC_PATH_PROG(dotlockfilepath, dotlockfile, no)
if test "$ac_cv_lib_lockfile_lockfile_create" != "no" ; then
  if test "$ac_cv_path_dotlockfilepath" != "no" ; then
    AC_CACHE_CHECK(whether dotlockfile is setgid, nmh_cv_dotlockfile_setgid,
    [ if test -g "$ac_cv_path_dotlockfilepath" ; then
        nmh_cv_dotlockfile_setgid=yes
      else
        nmh_cv_dotlockfile_setgid=no
      fi])
  fi
fi

dnl Provide a way for distcheck to disable setgid_mail via
dnl DISTCHECK_CONFIGURE_FLAGS.
if test x"$DISABLE_SETGID_MAIL" != x -a x"$DISABLE_SETGID_MAIL" != x0; then
  nmh_cv_dotlockfile_setgid=yes
fi

dnl If mailspool is not world-writable and dotlockfile is not setgid,
dnl we need to #define MAILGROUP to 1 and make inc setgid.
if test x"$LOCKTYPE" = x"dot" -a x"$nmh_cv_mailspool_world_writable" = x"no" -a x"$nmh_cv_dotlockfile_setgid" != x"yes" ; then
  dnl do we really need both of these?
  AC_DEFINE(MAILGROUP,1,
    [Define to 1 if you need to make `inc' set-group-id because your mail spool is not world writable. There are no guarantees as to the safety of doing this, but this #define will add some extra security checks.])dnl
  SETGID_MAIL=1
fi
AC_SUBST(SETGID_MAIL)dnl

dnl Use ls to see which group owns the mail spool directory.
AC_CACHE_CHECK(what group owns the mail spool, nmh_cv_ls_mail_grp,
[nmh_cv_ls_mail_grp=`$lspath -dL $nmh_cv_ls_grpopt $mailspool|$AWK '{print $4}'`
])
MAIL_SPOOL_GRP=$nmh_cv_ls_mail_grp
AC_SUBST(MAIL_SPOOL_GRP)dnl

dnl ------------------
dnl CHECK HEADER FILES
dnl ------------------

dnl On glibc we need to define at least the '_XOPEN_SOURCE' level of features,
dnl or wchar.h doesn't declare a prototype for wcwidth(). But if we only define
dnl that level then db.h won't compile. So we define _GNU_SOURCE which turns
dnl on everything. Perhaps other OSes need some feature switch set to get wcwidth()
dnl declared; if so they should have an entry added to this case statement.
dnl NB that we must define this on the compiler command line, not in config.h,
dnl because it must be set before any system header is included and there's no
dnl portable way to make sure that files generated by lex include config.h
dnl before system header files.

AS_CASE(["$host_os"],
	[linux*],
	[# Like DEFS, but doesn't get stomped on by configure when using config.h:
	  AS_IF([test -z "$CPPFLAGS"],[CPPFLAGS="-D_GNU_SOURCE"],
                [CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"])])

AC_HEADER_STDC
AC_HEADER_TIOCGWINSZ
AC_CHECK_HEADERS([errno.h fcntl.h crypt.h ncurses/termcap.h termcap.h \
                  langinfo.h wchar.h wctype.h iconv.h netdb.h \
                  sys/param.h sys/time.h sys/stream.h])

dnl
dnl Checks for _IO_write_ptr. A Linuxism used by nmh on linux. We
dnl really use a whole set of them, but this check should be
dnl sufficient.
dnl
AC_CHECK_HEADER(libio.h, [
  AC_EGREP_HEADER(_IO_write_ptr, libio.h, [
    AC_DEFINE(LINUX_STDIO,1,[Use the Linux _IO_*_ptr defines from <libio.h>.]) ]) ])

AC_CHECK_HEADER([sys/ptem.h], AC_DEFINE(WINSIZE_IN_PTEM,1,
  [Define to 1 if `struct winsize' requires <sys/ptem.h>.]),,
[[#if HAVE_SYS_STREAM_H
#  include <sys/stream.h>
#endif
]])

dnl ---------------
dnl CHECK FUNCTIONS
dnl ---------------
AC_CHECK_FUNCS([wcwidth mbtowc writev lstat nl_langinfo getutxent])

dnl Check for multibyte character set support
AS_IF([test "x$ac_cv_header_wchar_h" = "xyes" -a \
	    "x$ac_cv_header_wctype_h" = "xyes" -a \
	    "x$ac_cv_func_wcwidth" = "xyes" -a \
	    "x$ac_cv_func_mbtowc" = "xyes"],
      [AC_DEFINE([MULTIBYTE_SUPPORT], [1],
		 [Define to enable support for multibyte character sets.])
       MULTIBYTE_ENABLED=1],
      [MULTIBYTE_ENABLED=0])
AC_SUBST([MULTIBYTE_ENABLED])

dnl -------------------
dnl CHECK FOR LIBRARIES
dnl -------------------
dnl Check location of modf
AC_CHECK_FUNC([modf], , AC_CHECK_LIB([m], [modf]))

dnl Checks for network libraries (nsl, socket)
NMH_CHECK_NETLIBS

NMH_READLINE

termcap_curses_order="termcap curses ncurses"
for lib in $termcap_curses_order; do
  AC_CHECK_LIB(${lib}, tgetent, [TERMLIB="-l$lib"; break])
done
AC_SUBST([TERMLIB])dnl
AS_IF([test "x$TERMLIB" = "x"],
      [AC_MSG_FAILURE([Could not find tgetent in any library.  Is there a curses
or ncurses library or package that you can install?])])


dnl ---------------
dnl CHECK FOR ICONV
dnl ---------------

dnl Find iconv. It may be in libiconv and may be iconv() or libiconv()
if test "x$ac_cv_header_iconv_h" = "xyes"; then
  AC_CHECK_FUNC(iconv, ac_found_iconv=yes, ac_found_iconv=no)
  if test "x$ac_found_iconv" = "xno"; then
    AC_CHECK_LIB(iconv, iconv, ac_found_iconv=yes)
    if test "x$ac_found_iconv" = "xno"; then
      AC_CHECK_LIB(iconv, libiconv, ac_found_iconv=yes)
    fi
    if test "x$ac_found_iconv" != "xno"; then
      ICONVLIB="-liconv"
    fi
  else
    dnl Handle case where there is a native iconv but iconv.h is from libiconv
    AC_CHECK_DECL(_libiconv_version,
      [ AC_CHECK_LIB(iconv, libiconv, LIBS="-liconv $LIBS") ],,
      [ #include <iconv.h> ])
  fi
fi
if test "x$ac_found_iconv" = xyes; then
  AC_DEFINE([HAVE_ICONV], [1], [Define if you have the iconv() function.])
fi
AC_SUBST([ICONVLIB])

dnl Check if iconv uses const in prototype declaration
if test "x$ac_found_iconv" = "xyes"; then
  AC_CACHE_CHECK(for iconv declaration, ac_cv_iconv_const,
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
      [ac_cv_iconv_const=],
      [ac_cv_iconv_const=const])])
  AC_DEFINE_UNQUOTED([ICONV_CONST], $ac_cv_iconv_const,
    [Define as const if the declaration of iconv() needs const.])
fi

dnl --------------
dnl CHECK FOR NDBM
dnl --------------

AC_ARG_WITH([ndbm],AS_HELP_STRING([--with-ndbm=ARG],[use -lARG to link with ndbm]),
            [nmh_ndbm=$withval],[nmh_ndbm=autodetect])
AC_ARG_WITH([ndbmheader],AS_HELP_STRING([--with-ndbmheader=ARG],[#include <ARG> to use ndbm]),
            [nmh_ndbmheader=$withval],[nmh_ndbmheader=autodetect])

if test "$nmh_ndbm" = "autodetect"; then
  if test "$nmh_ndbmheader" != "autodetect"; then
    AC_MSG_ERROR([must specify both --with-ndbm and --with-ndbmheader or neither])
  else

    dnl There are at least four implementations of ndbm, and
    dnl several of those can be in different places at the whim
    dnl of the system integrator. A good summary of this mess
    dnl can be found at http://www.unixpapa.com/incnote/dbm.html

    dnl Classic ndbm with no library required (eg NetBSD): try this
    dnl first so we don't accidentally link in a pointless but harmless
    dnl library in one of the later ndbm.h+libfoo tests:
    NMH_CHECK_NDBM(ndbm.h,,,
    dnl Berkeley DBv2 emulating ndbm: header in db.h:
      NMH_CHECK_NDBM(db.h,db,,
    dnl Berkeley DBv1 emulating ndbm:
        NMH_CHECK_NDBM(ndbm.h,db,,
          NMH_CHECK_NDBM(ndbm.h,db1,,
    dnl Classic ndbm:
            NMH_CHECK_NDBM(ndbm.h,ndbm,,
    dnl glibc2.1 systems put db1 in a subdir:
              NMH_CHECK_NDBM(db1/ndbm.h,db1,,
    dnl GNU gdbm emulating ndbm, with header possibly in gdbm/
    dnl and possibly needing gbdm_compat library:
                NMH_CHECK_NDBM(gdbm/ndbm.h,gdbm,,
                  NMH_CHECK_NDBM(gdbm/ndbm.h,gdbm_compat -lgdbm,,
                    NMH_CHECK_NDBM(ndbm.h,gdbm,,
                      NMH_CHECK_NDBM(ndbm.h,gdbm_compat -lgdbm))))))))))

  fi
else
  dnl We don't really need to check that the user-specified values work,
  dnl but it is a convenience to the user to bomb out early rather than
  dnl after configure and half the compile process.
  NMH_CHECK_NDBM([$nmh_ndbmheader],[$nmh_ndbm])
fi

if test "$nmh_ndbm_found" = "no"; then
  AC_MSG_ERROR([could not find a working ndbm library/header combination])
else
  dnl Now export the lib/header to our makefile/config.h:
  if test x"$nmh_ndbmheader" != x; then
    AC_DEFINE_UNQUOTED(NDBM_HEADER, <$nmh_ndbmheader>,
      [Define to the header containing the ndbm API prototypes.])
  fi
  if test x"$nmh_ndbm" != x; then
    NDBM_LIBS="-l$nmh_ndbm"
  else
    NDBM_LIBS=
  fi
  AC_SUBST([NDBM_LIBS])
fi

dnl ------------------
dnl Set RPM build root
dnl ------------------
dnl nmhrpm is used in the final summary, see below.  The default value is
dnl reported there as ./RPM, consistent with the reporting of the default
dnl source code location as ., but its absolute path is used in the Makefile.
AC_ARG_WITH(rpmdir,
  AS_HELP_STRING([--with-rpmdir=RPMDIR], [RPM build directory @<:@RPM@:>@]))
  AS_IF([test x"$with_rpmdir" = x], [rpmdir='$(abs_srcdir)/RPM'; nmhrpm=./RPM],
          [rpmdir="$with_rpmdir"; eval "nmhrpm=${rpmdir}"])
AC_SUBST([rpmdir])


dnl --------------------
dnl CHECK FOR CYRUS-SASL
dnl --------------------

AS_IF([test x"$sasl_support" = x"yes"],[
  AC_CHECK_HEADER([sasl/sasl.h], , [AC_MSG_ERROR([sasl.h not found])])
  AC_CHECK_LIB([sasl2], [sasl_client_new], [SASLLIB="-lsasl2"],
    [AC_MSG_ERROR([Cyrus SASL library not found])])],[SASLLIB=""])
AC_SUBST([SASLLIB])

dnl -----------------
dnl CHECK FOR OPENSSL
dnl -----------------

AS_IF([test x"$tls_support" = x"yes"],[
  AC_CHECK_HEADER([openssl/ssl.h], , [AC_MSG_ERROR([openssl/ssl.h not found])])
  AC_CHECK_LIB([crypto], [BIO_write], ,
    [AC_MSG_ERROR([OpenSSL crypto library not found])])
  AC_CHECK_LIB([ssl], [SSL_library_init], ,
    [AC_MSG_ERROR([OpenSSL library not found])])])

dnl ---------------------
dnl CHECK TERMCAP LIBRARY
dnl ---------------------

dnl Add the termcap library, so that the following configure
dnl tests will find it when it tries to link test programs.
nmh_save_LIBS="$LIBS"
LIBS="$TERMLIB $LIBS"

dnl Check if tgetent accepts NULL (and will allocate its own termcap buffer)
dnl Some termcaps reportedly accept a zero buffer, but then dump core
dnl in tgetstr().
dnl Under Cygwin test program crashes but exit code is still 0. So,
dnl we test for a file that porgram should create
AH_TEMPLATE([TGETENT_ACCEPTS_NULL],
[Define to 1 if tgetent() accepts NULL as a buffer.])
AC_CACHE_CHECK(if tgetent accepts NULL,
nmh_cv_func_tgetent_accepts_null,
[AC_TRY_RUN([
main()
{
    char buf[4096];
    int r1 = tgetent(buf, "vt100");
    int r2 = tgetent((char*)0,"vt100");
    if (r1 >= 0 && r1 == r2) {
        char tbuf[1024], *u;
        u = tbuf;
    	tgetstr("cl", &u);
	creat("conftest.tgetent", 0640);
    }
    exit((r1 != r2) || r2 == -1);
}
],
  if test -f conftest.tgetent; then
    nmh_cv_func_tgetent_accepts_null=yes
  else
    nmh_cv_func_tgetent_accepts_null=no
  fi,
  nmh_cv_func_tgetent_accepts_null=no,
  nmh_cv_func_tgetent_accepts_null=no)])
if test x$nmh_cv_func_tgetent_accepts_null = xyes; then
  AC_DEFINE(TGETENT_ACCEPTS_NULL)
fi
AC_CACHE_CHECK(if tgetent returns 0 on success,
nmh_cv_func_tgetent_zero_success,
[AC_TRY_RUN([
main()
{
    char buf[4096];
    int r1 = tgetent(buf, "!@#$%^&*");
    int r2 = tgetent(buf, "vt100");
    if (r1 < 0 && r2 == 0) {
        char tbuf[1024], *u;
        u = tbuf;
    	tgetstr("cl", &u);
	creat("conftest.tgetent0", 0640);
    }
    exit(r1 == r2);
}
],
  if test -f conftest.tgetent0; then
    nmh_cv_func_tgetent_zero_success=yes
  else
    nmh_cv_func_tgetent_zero_success=no
  fi,
  nmh_cv_func_tgetent_zero_success=no,
  nmh_cv_func_tgetent_zero_success=no)])
AH_TEMPLATE([TGETENT_SUCCESS],
[Define to what tgetent() returns on success (0 on HP-UX X/Open curses).])
if test x$nmh_cv_func_tgetent_zero_success = xyes; then
  AC_DEFINE(TGETENT_SUCCESS, 0)
else
  AC_DEFINE(TGETENT_SUCCESS, 1)
fi

dnl Now put the libraries back to what it was before we
dnl starting checking the termcap library.
LIBS="$nmh_save_LIBS"

dnl --------------
dnl CHECK TYPEDEFS
dnl --------------
AC_TYPE_PID_T
AC_TYPE_OFF_T
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_SIZE_T

dnl ----------------
dnl CHECK STRUCTURES
dnl ----------------

dnl For platforms such as FreeBSD that have tm_gmtoff in struct tm.
dnl (FreeBSD has a timezone() function but not a timezone global
dnl variable that is visible).
AC_CHECK_MEMBERS([struct tm.tm_gmtoff],,,[#include <time.h>])

AC_STRUCT_DIRENT_D_TYPE

dnl Where is <signal.h> located?  Needed as input for signames.awk
AC_CACHE_CHECK(where signal.h is located, nmh_cv_path_signal_h,
[for SIGNAL_H in /usr/include/bsd/sys/signal.h  dnl Next
                 /usr/include/asm/signal.h      dnl Linux 1.3.0 and above
                 /usr/include/asm/signum.h      dnl some versions of Linux/Alpha
                 /usr/include/linux/signal.h    dnl Linux up to 1.2.11
                 /usr/include/sys/signal.h      dnl Almost everybody else
                 /dev/null;                     dnl Just in case we fall through
do
  test -f $SIGNAL_H && \
  grep '#[ 	]*define[ 	][ 	]*SIG[0-9A-Z]*[ 	]*[0-9][0-9]*' $SIGNAL_H > /dev/null && \
  break
done
nmh_cv_path_signal_h=$SIGNAL_H
])
SIGNAL_H=$nmh_cv_path_signal_h
AC_SUBST(SIGNAL_H)dnl

dnl
dnl Sigh, this is required because under the new world order autoconf has
dnl nothing to create in a few of the build directories when doing an object
dnl tree build.  So make sure we created certain directories if they don't
dnl exist.
dnl

AC_CONFIG_COMMANDS([build-directories],
[test -d etc || ${MKDIR_P} etc
test -d man || ${MKDIR_P} man])

AC_CONFIG_COMMANDS_POST([

dnl These odd looking assignments are done to expand out unexpanded
dnl variables in bindir et al (for instance mandir is '${datarootdir}/man'),
dnl but expanding that gives '${prefix}/share/man', so we need to expand
dnl again to get the final answer.
dnl We only use the expanded versions to print the install paths in
dnl the final summary and should use them nowhere else (see the autoconf
dnl docs for the rationale for bindir etc being unexpanded).
eval "nmhbin=${bindir}";         eval "nmhbin=${nmhbin}"
eval "nmhsysconf=${sysconfdir}"; eval "nmhsysconf=${nmhsysconf}"
eval "nmhlib=${libdir}";         eval "nmhlib=${nmhlib}"
eval "nmhman=${mandir}";         eval "nmhman=${nmhman}"
eval "nmhrpm=${nmhrpm}";

AC_MSG_NOTICE([
nmh configuration
-----------------
nmh version                : AC_PACKAGE_VERSION
host os                    : ${host}
compiler                   : ${CC}
compiler flags             : ${CFLAGS}
linker flags               : ${LDFLAGS}
preprocessor flags         : ${CPPFLAGS}
source code location       : ${srcdir}
binary install path        : ${nmhbin}
library install path       : ${nmhlib}
config files install path  : ${nmhsysconf}
man page install path      : ${nmhman}
RPM build root             : ${nmhrpm}
backup prefix              : ${backup_prefix}
transport system           : ${MTS}
file locking type          : ${LOCKTYPE}
default smtp servers       : ${smtpservers}
default editor             : ${editorpath}
default pager              : ${pagerpath}
SASL support               : ${sasl_support}
TLS support                : ${tls_support}
])])dnl

dnl ---------------
dnl OUTPUT MAKEFILE
dnl ---------------
AC_CONFIG_FILES([Makefile uip/spost test/common.sh])
AC_OUTPUT
