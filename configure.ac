dnl
dnl configure.ac -- autoconf template for nmh
dnl

dnl Move this up a bit
AC_PREREQ([2.68])

AC_INIT([nmh], m4_normalize(m4_include([VERSION])), [nmh-workers@nongnu.org])
AC_CONFIG_SRCDIR([h/nmh.h])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([-Wall foreign serial-tests subdir-objects 1.12])

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
AS_IF([test x"$with_tls" != x"no"],[
      AC_DEFINE([TLS_SUPPORT], [1], [Support TLS for session encryption.])dnl
      tls_support=yes],[tls_support=no])

dnl Set the backup prefix
AC_ARG_WITH([hash-backup],
  AS_HELP_STRING([--with-hash-backup],[use # as the backup prefix (default: ,)]))
AS_IF([test x"$with_hash_backup" != x -a x"$with_hash_backup" != x"no"],
      [backup_prefix="#"], [backup_prefix=","])
AC_DEFINE_UNQUOTED([BACKUP_PREFIX], "$backup_prefix",
    [The prefix that is prepended to the name of message files when they are "removed" by rmm. This should typically be `,' or `#'.])dnl

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

dnl What should be the default mail server(s)?
AC_ARG_WITH([smtpservers],
  [AS_HELP_STRING([--with-smtpservers='SMTPSERVER1@<:@ SMTPSERVER2...@:>@'],
  [specify the default SMTP server(s) @<:@localhost@:>@])])
AS_IF([test -n "$with_smtpservers"], [smtpservers="$with_smtpservers"],
      [smtpservers="localhost"])
AC_SUBST([smtpservers])dnl

dnl -------------------------------------------------------------------
dnl Default location is /usr/local/nmh/{bin,etc,libexec,man}, unless we
dnl find an existing installation, in which case we use its location.
dnl -------------------------------------------------------------------
AC_PREFIX_DEFAULT([/usr/local/nmh])
AC_PREFIX_PROGRAM([mhparam])

dnl ------------------
dnl CHECK THE COMPILER
dnl ------------------

AC_PROG_CC([cc gcc])
AM_PROG_CC_C_O

dnl ----------------
dnl CUSTOMIZE CFLAGS
dnl ----------------
dnl The user didn't specify CFLAGS, so customize them.
if test "$GCC" != yes; then
  dnl Use -v and some other options with SunStudio cc.  lex produces
  dnl code that causes unreachable-statement warnings.  It might be
  dnl better to use an autoconf test, except -v will probably succeed
  dnl with many other compilers but have different meaning.
  AS_CASE([`${CC} -V 2>&1`],
          [cc:\ Sun\ C*], [CFLAGS=\
"${CFLAGS:+$CFLAGS }-v -errtags=yes -erroff=E_STATEMENT_NOT_REACHED"])
fi

AC_CACHE_CHECK([whether compiler supports -Wall], [nmh_cv_has_wall],
               [nmh_saved_cflags="$CFLAGS"
               CFLAGS="$CFLAGS -Wall -Werror"
               AC_TRY_COMPILE([],[],nmh_cv_has_wall=yes,nmh_cv_has_wall=no)
               CFLAGS="$nmh_saved_cflags"])
test "$nmh_cv_has_wall" = "yes"  &&  CFLAGS="${CFLAGS:+$CFLAGS }-Wall"

AC_CACHE_CHECK([whether compiler supports -Wextra], [nmh_cv_has_wextra],
               [nmh_saved_cflags="$CFLAGS"
               CFLAGS="$CFLAGS -Wextra -Werror"
               AC_TRY_COMPILE([],[],nmh_cv_has_wextra=yes,
                              nmh_cv_has_wextra=no)
               CFLAGS="$nmh_saved_cflags"])
test "$nmh_cv_has_wextra" = "yes"  &&  CFLAGS="${CFLAGS:+$CFLAGS }-Wextra"

if test "$GCC" = yes; then
  dnl Do this regardless of auto_cflags value.
  dnl Enable _FORTIFY_SOURCE checking, which is supported by
  dnl gcc 4.1 and later, but only when optimization is enabled.
  dnl It shouldn't hurt with earlier versions because it's a cpp macro.
  dnl If the user configures with a CFLAGS that contains a -O flag
  dnl but then removes it a make time, they'll also have to remove
  dnl -D_FORTIFY_SOURCE from CPPFLAGS because it requires optimization.
  AS_CASE([$CFLAGS],
          [-O*|*' -O'*],[CPPFLAGS="${CPPFLAGS:+$CPPFLAGS }-D_FORTIFY_SOURCE=2"])
fi

dnl FreeBSD needs a bit of magic to bring getline() into scope.
dnl We do this here rather than in (say) h/mh.h because this macro must
dnl be defined before <stdio.h> is pulled in.
dnl
dnl And while we're here, add the packages tree to the cpp and ld search
dnl paths.  Note that FreeBSD's pkg(8) seems to be hardwired to use /usr/local.
dnl If /usr/ports is installed, we use its idea of where things are installed,
dnl otherwise we assume /usr/local.

AS_IF([test `uname` = FreeBSD],
	[CPPFLAGS="${CPPFLAGS:+$CPPFLAGS }-D_WITH_GETLINE"
	 nmh_cv_freebsd_localbase=`echo '.include <bsd.port.mk>' \
		| make -k -f /dev/stdin -V LOCALBASE 2>/dev/null | sed 1q`
	 test -z "$nmh_cv_freebsd_localbase" && nmh_cv_freebsd_localbase=/usr/local
	 CPPFLAGS="${CPPFLAGS:+$CPPFLAGS }-I$nmh_cv_freebsd_localbase/include"
	 LDFLAGS="${LDFLAGS:+$LDFLAGS }-L$nmh_cv_freebsd_localbase/lib"
])

dnl --------------
dnl CUSTOMIZE LINK
dnl --------------
AS_IF([test `uname` = OpenBSD],
      dnl Munge "often/almost always misused" warnings from OpenBSD linker
      dnl so that they don't color the waterfall.
      [POSTLINK="2>&1 | sed -e \
's/: w\(arning: s.*() is .* misused, please use\)/: W\1/'"])
AC_SUBST([POSTLINK])

dnl -----------------
dnl CUSTOMIZE LDFLAGS
dnl -----------------
dnl Disable clang complaint about unused -ansi when linking.
AC_CACHE_CHECK([whether linker supports -Qunused-arguments],
               [nmh_cv_has_q_unused_arguments],
               [AS_IF([test "$GCC" = yes  &&  `${CC} --version 2>&1 | \
                       grep clang >/dev/null`],
                      [nmh_saved_ldflags="$LDFLAGS"
                       LDFLAGS="$LDFLAGS -Qunused-arguments"
                       AC_TRY_LINK([],[],nmh_cv_has_q_unused_arguments=yes,
                                         nmh_cv_has_q_unused_arguments=no)
                       LDFLAGS="$nmh_saved_ldflags"],
                       [nmh_cv_has_q_unused_arguments=no])])
test "$nmh_cv_has_q_unused_arguments" = "yes"  &&  \
  LDFLAGS="${LDFLAGS:+$LDFLAGS }-Qunused-arguments"

dnl This hack turns off assertions by default, assuming
dnl that configure still uses this shell variable.  Without
dnl it, AC_HEADER_ASSERT enables assertions by default.
test -z "$enable_assert"  &&  enable_assert=no

AC_HEADER_ASSERT

dnl ------------------
dnl CHECK FOR PROGRAMS
dnl ------------------
AC_PROG_MAKE_SET        dnl Does make define $MAKE
AC_PROG_INSTALL         dnl Check for BSD compatible `install'
AC_PROG_RANLIB          dnl Check for `ranlib'
AC_PROG_AWK             dnl Check for mawk,gawk,nawk, then awk
AC_PROG_SED             dnl Check for Posix-compliant sed
AC_PROG_YACC            dnl Check for yacc/bison
AM_PROG_LEX             dnl Check for lex/flex

AM_PROG_AR

dnl Look for `cut'
pathtmp=/usr/xpg4/bin:/usr/bin:/bin:/usr/local/bin:/usr/ucb
AC_PATH_PROG([cutpath], [cut], [no], [$pathtmp])

dnl
dnl Check for MD5 program and formatting command
dnl
AC_CHECK_PROGS([MD5SUM], [md5sum md5], [missing])
AS_CASE(["${MD5SUM}"],
	[md5sum], [MD5FMT="cat"],
	[md5], [[MD5FMT="${SED} -e 's/MD5 *(.*) *= \([0-9a-f]*\)/\1/'"]],
	[MD5FMT="missing"])
AC_SUBST([MD5FMT])

dnl Look for `ls'
pathtmp=/usr/xpg4/bin:/usr/bin:/bin:/usr/local/bin:/usr/ucb
AC_PATH_PROG([lspath], [ls], [no], [$pathtmp])

dnl See how we get ls to display the owner and the group
AS_IF([test "$lspath" != "no"],
  [AC_CACHE_CHECK([how to get ls to show us the group ownership of a file],
		 [nmh_cv_ls_grpopt],
  [AS_IF([test x"`$lspath -dl / | $AWK '{print $9}'`" = x"/"],[
    dnl There were 9 parameters, so unless this is a really bizarre, nonstandard
    dnl ls, it would seem -l gave us both the user and group.  On this type of
    dnl ls, -g makes _only_ the group be displayed (and not the user).
    nmh_cv_ls_grpopt="-l"],[
    dnl Looks like -l only gave us the user, so we need -g to get the group too.
    nmh_cv_ls_grpopt="-lg"])])])

dnl Look for `sendmail'
pathtmp=/usr/sbin:/usr/lib:/usr/etc:/usr/ucblib:/usr/bin:/bin
AC_PATH_PROG([sendmailpath], [sendmail], [/usr/sbin/sendmail], [$pathtmp])

dnl Cygwin FAT filesystems do not support hard links.  So default to
dnl cp instead, even if running on an NTFS or other filesystem.
AS_CASE(["$host_os"],
  [cygwin*], [LN=cp],
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
AS_IF([test x"$DISABLE_SETGID_MAIL" != x -a x"$DISABLE_SETGID_MAIL" != x0],
  [nmh_cv_dotlockfile_setgid=yes])

dnl If mailspool is not world-writable and dotlockfile is not setgid,
dnl we need to #define MAILGROUP to 1 and make inc setgid.
if test x"$with_locking" = x"dot" -a x"$nmh_cv_mailspool_world_writable" = x"no" -a x"$nmh_cv_dotlockfile_setgid" != x"yes" ; then
  dnl do we really need both of these?
  AC_DEFINE([MAILGROUP],[1],
    [Define to 1 if you need to make `inc' set-group-id because your mail spool is not world writable. There are no guarantees as to the safety of doing this, but this #define will add some extra security checks.])dnl
  SETGID_MAIL=1
fi
AC_SUBST([SETGID_MAIL])dnl

dnl Use ls to see which group owns the mail spool directory.
AC_CACHE_CHECK(what group owns the mail spool, nmh_cv_ls_mail_grp,
[nmh_cv_ls_mail_grp=`$lspath -dL $nmh_cv_ls_grpopt $mailspool|$AWK '{print $4}'`
])
MAIL_SPOOL_GRP=$nmh_cv_ls_mail_grp
AC_SUBST([MAIL_SPOOL_GRP])dnl

NMH_MIMETYPEPROC
NMH_MIMEENCODINGPROC

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
	CPPFLAGS="${CPPFLAGS:+$CPPFLAGS }-D_GNU_SOURCE"])

AC_HEADER_TIOCGWINSZ
AC_CHECK_HEADERS([fcntl.h wchar.h wctype.h sys/param.h sys/time.h sys/stream.h])

AC_CHECK_HEADER([sys/ptem.h], AC_DEFINE(WINSIZE_IN_PTEM,1,
  [Define to 1 if `struct winsize' requires <sys/ptem.h>.]),,
[[#if HAVE_SYS_STREAM_H
#  include <sys/stream.h>
#endif
]])

dnl ---------------
dnl CHECK FUNCTIONS
dnl ---------------
AC_CHECK_FUNCS([wcwidth mbtowc getutxent arc4random mkstemps])

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
dnl Checks for network libraries (nsl, socket)
NMH_CHECK_NETLIBS

dnl Check the locking functions supported and what we should use by default
NMH_LOCKING

dnl Check for iconv
NMH_CHECK_ICONV

dnl Check for tputs() callback argument
NMH_TPUTS_PUTC_ARG

termcap_curses_order="termcap curses ncurses"
for lib in $termcap_curses_order; do
  AC_CHECK_LIB([${lib}], [setupterm], [TERMLIB="-l$lib"; break])
done
AC_SUBST([TERMLIB])dnl
AS_IF([test "x$TERMLIB" = "x"],
      [AC_MSG_FAILURE([Could not find setupterm in any library.  Is there a
curses or ncurses library or package that you can install?])])

dnl Check for readline support
NMH_READLINE

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
    dnl Berkeley DBv2 emulating ndbm: header in db.h, e.g., 32-bit Cygwin:
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
    dnl 64-bit Cygwin:
                      NMH_CHECK_NDBM(ndbm.h,gdbm_compat -lgdbm,,
    dnl On Linux, libgdbm_compat.so loads libgdbm.so as well, so it doesn't
    dnl need to be explicit:
                        NMH_CHECK_NDBM(gdbm-ndbm.h,gdbm_compat)))))))))))
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
AC_ARG_WITH([rpmdir],
  [AS_HELP_STRING([--with-rpmdir=RPMDIR], [RPM build directory @<:@RPM@:>@])])
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
  dnl OpenBSD 5 needs the other-libraries (fourth argument) to the
  dnl AC_CHECK_LIB for SSL_library_init, because it doesn't
  dnl automatically append -lcrypto when linking with -lssl.
  AC_CHECK_HEADER([openssl/ssl.h], , [AC_MSG_ERROR([openssl/ssl.h not found])])
  AC_CHECK_LIB([crypto], [BIO_write], [TLSLIB="-lcrypto"],
    [AC_MSG_ERROR([OpenSSL crypto library not found])])
  AC_CHECK_LIB([ssl], [SSL_library_init], [TLSLIB="-lssl $TLSLIB"],
    [AC_MSG_ERROR([OpenSSL library not found])],[$TLSLIB])],
  [TLSLIB=])
AC_SUBST([TLSLIB])

dnl ----------------
dnl CHECK FLEX FIXUP
dnl ----------------
dnl Use LFLAGS make variable setting to work around bugs in flex
dnl 2.5.36-37 that cause signed/unsigned mismatch,
dnl http://sourceforge.net/p/flex/bugs/140/
AS_IF([test "$LEX" = flex],
      [AS_CASE([`$LEX -V`],
               [flex\ 2.5.35], [LFLAGS=\
'; sed "s/	int n;/	size_t n;/" $@ >$@.tmp && mv -f $@.tmp $@'],
               [flex\ 2.5.3[[67]]], [LFLAGS=\
'; sed "s/\(	\)int i;/\1yy_size_t i;/" $@ >$@.tmp && mv -f $@.tmp $@'])
       AC_SUBST([LFLAGS])])

dnl ------------------
dnl FOR INTERNAL USE by the NMH test suite
dnl ------------------
AC_ARG_VAR([NMHETCDIRINST], [for internal use by nmh test suite])
AS_IF([test -n "$NMHETCDIRINST"], [nmhetcdirinst=$NMHETCDIRINST]
      AC_SUBST([nmhetcdirinst]))

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
[test -d man || AS_MKDIR_P([man])])

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
eval "nmhlibexec=${libexecdir}"; eval "nmhlibexec=${nmhlibexec}"
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
libexec install path       : ${nmhlibexec}/nmh
config files install path  : ${nmhsysconf}/nmh
man page install path      : ${nmhman}
RPM build root             : ${nmhrpm}
backup prefix              : ${backup_prefix}
transport system           : ${MTS}
spool default locking type : ${with_locking}
default smtp servers       : ${smtpservers}
SASL support               : ${sasl_support}
TLS support                : ${tls_support}
])])dnl

dnl ---------------
dnl OUTPUT MAKEFILE
dnl ---------------
AC_CONFIG_FILES([Makefile test/common.sh])
AC_CONFIG_FILES([uip/spost], [chmod +x uip/spost])
AC_OUTPUT
