
/****** BEGIN USER CONFIGURATION SECTION *****/

/*
 * IMPORTANT: UNCOMMENT THE DEFINES FOR YOUR OPERATING SYSTEM
 *
 * These are slowly being phased out, but currently
 * not everyone is auto-configured.  Then decide if you
 * wish to change the features that are compiled into nmh.
 */

/*
 *  Solaris 2.x
 *  Irix
 *  OSF/1
 *  HP-UX
 *  AIX
 */
/* #define SYS5  1 */
/* #define SVR4  1 */

/*
 *  SunOS 4.1.x
 */
/* #define BIND     1 */
/* #define BSD42    1 */

/*
 *  Linux
 */
/* #define LINUX_STDIO  1 */

/*
 *  FreeBSD 2.x
 *  NetBSD 1.x,
 *  OpenBSD 2.x
 *  BSD/OS 2.x
 */
/* #define BIND     1 */
/* #define BSD42    1 */
/* #define BSD44    1 */

/*
 *  SCO 4.x
 *  SCO 5.x
 *
 * I believe the second `define' is only necessary
 * for SCO 5.x, not SCO 4.x
 */
/* #define SYS5         1 */
/* #define SCO_5_STDIO  1 */

/*
 * Define to 1 if you need to make `inc' set-group-id
 * because your mail spool is not world writable.  This
 * will add some extra security checks, although I can't
 * guarantee it is safe.  Also, you will need to change the
 * group and add the setgid bit to `inc' manually after
 * installation.
 */
/* #define MAILGROUP  1 */

/*
 * Turn on locale (setlocale) support
 */
#define LOCALE  1

/*
 * Define to 1 the type of file locking to use.  You need to
 * make sure the type of locking you use is compatible with
 * other programs which may modify your maildrops.
 * Currently you can only use one type.
 */
#define DOT_LOCKING   1
/* #define FCNTL_LOCKING 1 */
/* #define LOCKF_LOCKING 1 */
/* #define FLOCK_LOCKING 1 */

/*
 * If you have defined DOT_LOCKING, then the default is to
 * place the lock files in the same directory as the file that
 * is to be locked.  Alternately, if you define LOCKDIR, you
 * can specify that all lock files go in a specific directory.
 * Don't define this unless you know you need it.
 */
/* #define LOCKDIR "/usr/spool/locks" */

/*
 * Define this if your passwords are stored in some type of
 * distributed name service, such as NIS, or NIS+.
 */
#define DBMPWD  1

/*
 * Directs nmh not to try and rewrite addresses
 * to their official form.  You probably don't
 * want to change this without good reason.
 */
#define DUMB    1

/*
 * Define this if you do not want nmh to attach the local hostname
 * to local addresses.  You must also define DUMB.  You probably
 * dont' need this unless you are behind a firewall.
 */
/* #define REALLYDUMB  1 */

/*
 * Directs inc/slocal to extract the envelope sender from "From "
 * line.  If inc/slocal is saving message to folder, then this
 * sender information is then used to create a Return-Path
 * header which is then added to the message.
 */
#define RPATHS  1

/*
 * If defined, slocal will use `mbox' format when saving to
 * your standard mail spool.  If not defined, it will use
 * mmdf format.
 */
#define SLOCAL_MBOX  1

/*
 * If this is defined, nmh will recognize the ~ construct.
 */
#define MHRC    1

/*
 * Compile simple ftp client into mhn.  This will be used by mhn
 * for ftp access unless you have specified another access method
 * in your .mh_profile or mhn.defaults.  Use the "mhn-access-ftp"
 * profile entry to override this.  Check mhn(1) man page for
 * details.
 */
#define BUILTIN_FTP 1

/*
 * If you enable POP support, this is the the port name
 * that nmh will use.  Make sure this is defined in your
 * /etc/services file (or its NIS/NIS+ equivalent).  If you
 * are using KPOP, you will probably need to change this
 * to "kpop".
 */
#define POPSERVICE "pop3"

/*
 * Define the default creation modes for folders and messages.
 */
#define DEFAULT_FOLDER_MODE "0700"
#define DEFAULT_MESSAGE_MODE "0600"

/*
 * The prefix which is prepended to the name of messages when they
 * are "removed" by rmm.  This should typically be `,' or `#'
 */
#define BACKUP_PREFIX ","

/*
 * Name of link to file to which you are replying.
 */
#define LINK "@"

/*
 * If wait/waitpid returns an int (no union wait).
 */
#define WAITINT 1

/***** END USER CONFIGURATION SECTION *****/
@TOP@

/*
 * Define this if you want SMTP (simple mail transport protocol)
 * support.  When sending mail, instead of passing the message to
 * the mail transport agent (typically sendmail), nmh will open a
 * socket connection to the mail port on the machine specified in
 * the `mts.conf' file (default is localhost), and speak SMTP directly.
 */
#undef SMTPMTS

/*
 * Use sendmail as transport agent.  Post messages by piping
 * them directly to sendmail.
 */
#undef SENDMTS

/*
 * Define this to compile client-side support for pop into
 * inc and msgchk.  Do not change this value manually.  You
 * must run configure with the '--enable-nmh-pop' option
 * to correctly build the pop client support.
 */
#undef POP

/*
 * Define this to compile client-side support for kpop
 * (kerberized pop) into inc and msgchk.  Do not change this
 * value manually.  You must run configure with the option
 * '--with-krb4=PREFIX' to correctly build the kpop client support.
 */
#undef KPOP

/*
 * Define this to "pop" when using Kerberos V4
 */
#undef KPOP_PRINCIPAL

/*
 * Define this to compile support for using Hesiod to locate
 * pop servers into inc and msgchk.  Do not change this value
 * manually.  You must run configure with the option
 * '--with-hesiod=PREFIX' to correctly build Hesiod support.
 */
#undef HESIOD

/*
 * Compile in support for the Emacs front-end mh-e.
 */
#undef MHE

/* Define to 1 if your termcap library has the ospeed variable */
#undef HAVE_OSPEED
/* Define to 1 if you have ospeed, but it is not defined in termcap.h */
#undef MUST_DEFINE_OSPEED

/* Define to 1 if tgetent() accepts NULL as a buffer */
#undef TGETENT_ACCEPTS_NULL

/* Define to 1 if you have reliable signals */
#undef RELIABLE_SIGNALS

/* Define to 1 if you use POSIX style signal handling */
#undef POSIX_SIGNALS
 
/* Define to 1 if you use BSD style signal handling (and can block signals) */
#undef BSD_SIGNALS
 
/* Define to 1 if you use SYS style signal handling (and can block signals) */
#undef SYSV_SIGNALS
 
/* Define to 1 if you have no signal blocking at all (bummer) */
#undef NO_SIGNAL_BLOCKING

/* Define to `unsigned int' if <sys/types.h> or <signal.h> doesn't define */
#undef sigset_t

/*
 * Define to 1 if your vi has ATT bug, such that it returns
 * non-zero exit codes on `pseudo-errors'.
 */
#undef ATTVIBUG

/* Define ruserpass as _ruserpass if your libraries have a bug *
 * such that it can't find ruserpass, but can find _ruserpass. */
#undef ruserpass

/* Define if your system defines TIOCGWINSZ in sys/ioctl.h.  */
#undef GWINSZ_IN_SYS_IOCTL

/* Define if your system defines `struct winsize' in sys/ptem.h.  */
#undef WINSIZE_IN_PTEM

/* Define to 1 if struct tm has gmtoff */
#undef HAVE_TM_GMTOFF
