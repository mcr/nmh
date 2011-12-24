
/*
 * rcvtty.c -- a rcvmail program (a lot like rcvalert) handling IPC ttys
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/* Changed to use getutent() and friends.  Assumes that when getutent() exists,
 * a number of other things also exist.  Please check.
 * Ruud de Rooij <ruud@ruud.org>  Sun, 28 May 2000 17:28:55 +0200
 */

#include <h/mh.h>
#include <h/signals.h>
#include <h/rcvmail.h>
#include <h/scansbr.h>
#include <h/tws.h>
#include <h/mts.h>
#include <signal.h>
#include <fcntl.h>

#include <utmp.h>

#ifndef HAVE_GETUTENT
# ifndef UTMP_FILE
#  ifdef _PATH_UTMP
#   define UTMP_FILE _PATH_UTMP
#  else
#   define UTMP_FILE "/etc/utmp"
#  endif
# endif
#endif

#define	SCANFMT	\
"%2(hour{dtimenow}):%02(min{dtimenow}): %<(size)%5(size) %>%<{encrypted}E%>\
%<(mymbox{from})%<{to}To:%14(friendly{to})%>%>%<(zero)%17(friendly{from})%>  \
%{subject}%<{body}<<%{body}>>%>"

static struct swit switches[] = {
#define	BIFFSW	0
    { "biff", 0 },
#define	FORMSW	1
    { "form formatfile", 0 },
#define	FMTSW	2
    { "format string", 5 },
#define WIDTHSW 3
    { "width columns", 0 },
#define NLSW    4
    { "newline", 0 },
#define NNLSW   5
    { "nonewline", 0 },
#define BELSW	6
    { "bell", 0 },
#define	NBELSW	7
    { "nobell", 0 },
#define VERSIONSW 8
    { "version", 0 },
#define	HELPSW	9
    { "help", 0 },
    { NULL, 0 }
};

static jmp_buf myctx;
static int bell = 1;
static int newline = 1;
static int biff = 0;
static int width = 0;
static char *form = NULL;
static char *format = NULL;

/*
 * external prototypes
 */
char *getusername(void);

/*
 * static prototypes
 */
static RETSIGTYPE alrmser (int);
static int message_fd (char **);
static int header_fd (void);
static void alert (char *, int);


int
main (int argc, char **argv)
{
    int md, vecp = 0;
    char *cp, *user, buf[BUFSIZ], tty[BUFSIZ];
    char **argp, **arguments, *vec[MAXARGS];
#ifdef HAVE_GETUTENT
    struct utmp * utp;
#else
    struct utmp ut;
    register FILE *uf;
#endif

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    mts_init (invo_name);
    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    vec[vecp++] = --cp;
		    continue;

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [command ...]", invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case BIFFSW:
		    biff = 1;
		    continue;

		case FORMSW: 
		    if (!(form = *argp++) || *form == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    format = NULL;
		    continue;
		case FMTSW: 
		    if (!(format = *argp++) || *format == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    form = NULL;
		    continue;

		case WIDTHSW:
		    if (!(cp = *argp++) || *cp == '-')
			adios(NULL, "missing argument to %s", argp[-2]);
		    width = atoi(cp);
		    continue;
                case NLSW:
                    newline = 1;
                    continue;
                case NNLSW:
                    newline = 0;
                    continue;
                case BELSW:
                    bell = 1;
                    continue;
                case NBELSW:
                    bell = 0;
                    continue;

	    }
	}
	vec[vecp++] = cp;
    }
    vec[vecp] = 0;

    if ((md = vecp ? message_fd (vec) : header_fd ()) == NOTOK)
	exit (RCV_MBX);

    user = getusername();

#ifdef HAVE_GETUTENT
    setutent();
    while ((utp = getutent()) != NULL) {
        if (
#ifdef HAVE_STRUCT_UTMP_UT_TYPE
	       utp->ut_type == USER_PROCESS 
	       &&
#endif
               utp->ut_name[0] != 0
               && utp->ut_line[0] != 0
               && strncmp (user, utp->ut_name, sizeof(utp->ut_name)) == 0) {
            strncpy (tty, utp->ut_line, sizeof(utp->ut_line));
	    alert (tty, md);
        }
    }
    endutent();
#else
    if ((uf = fopen (UTMP_FILE, "r")) == NULL)
	exit (RCV_MBX);
    while (fread ((char *) &ut, sizeof(ut), 1, uf) == 1)
	if (ut.ut_name[0] != 0
		&& strncmp (user, ut.ut_name, sizeof(ut.ut_name)) == 0) {
	    strncpy (tty, ut.ut_line, sizeof(ut.ut_line));
	    alert (tty, md);
	}
    fclose (uf);
#endif

    exit (RCV_MOK);
    return 0;  /* dead code to satisfy the compiler */
}


static RETSIGTYPE
alrmser (int i)
{
#ifndef RELIABLE_SIGNALS
    SIGNAL (SIGALRM, alrmser);
#endif

    longjmp (myctx, 1);
}


static int
message_fd (char **vec)
{
    pid_t child_id;
    int bytes, fd, seconds;
    char tmpfil[BUFSIZ];
    struct stat st;

#ifdef HAVE_MKSTEMP
    fd = mkstemp (strncpy (tmpfil, "/tmp/rcvttyXXXXX", sizeof(tmpfil)));
#else
    unlink (mktemp (strncpy (tmpfil, "/tmp/rcvttyXXXXX", sizeof(tmpfil))));
    if ((fd = open (tmpfil, O_RDWR | O_CREAT | O_TRUNC, 0600)) == NOTOK)
	return header_fd ();
#endif
    unlink (tmpfil);

    if ((child_id = vfork()) == NOTOK) {
	/* fork error */
	close (fd);
	return header_fd ();
    } else if (child_id) {
	/* parent process */
	if (!setjmp (myctx)) {
	    SIGNAL (SIGALRM, alrmser);
	    bytes = fstat(fileno (stdin), &st) != NOTOK ? (int) st.st_size : 100;

	    /* amount of time to wait depends on message size */
	    if (bytes <= 100) {
		/* give at least 5 minutes */
		seconds = 300;
	    } else if (bytes >= 90000) {
		/* but 30 minutes should be long enough */
		seconds = 1800;
	    } else {
		seconds = (bytes / 60) + 300;
	    }
	    alarm ((unsigned int) seconds);
	    pidwait(child_id, OK);
	    alarm (0);

	    if (fstat (fd, &st) != NOTOK && st.st_size > (off_t) 0)
		return fd;
	} else {
	    /*
	     * Ruthlessly kill the child and anything
	     * else in its process group.
	     */
	    killpg(child_id, SIGKILL);
	}
	close (fd);
	return header_fd ();
    }

    /* child process */
    rewind (stdin);
    if (dup2 (fd, 1) == NOTOK || dup2 (fd, 2) == NOTOK)
	_exit (-1);
    closefds (3);
    setpgid ((pid_t) 0, getpid ());	/* put in own process group */
    execvp (vec[0], vec);
    _exit (-1);
    return 1;  /* dead code to satisfy compiler */
}


static int
header_fd (void)
{
    int fd;
    char *nfs;
    char *tfile = NULL;

    tfile = m_mktemp2(NULL, invo_name, &fd, NULL);
    if (tfile == NULL) return NOTOK;
    unlink (tfile);

    rewind (stdin);

    /* get new format string */
    nfs = new_fs (form, format, SCANFMT);
    scan (stdin, 0, 0, nfs, width, 0, 0, NULL, 0L, 0);
    if (newline)
        write (fd, "\n\r", 2);
    write (fd, scanl, strlen (scanl));
    if (bell)
        write (fd, "\007", 1);

    return fd;
}


static void
alert (char *tty, int md)
{
    int i, td, mask;
    char buffer[BUFSIZ], ttyspec[BUFSIZ];
    struct stat st;

    snprintf (ttyspec, sizeof(ttyspec), "/dev/%s", tty);

    /*
     * The mask depends on whether we are checking for
     * write permission based on `biff' or `mesg'.
     */
    mask = biff ? S_IEXEC : (S_IWRITE >> 3);
    if (stat (ttyspec, &st) == NOTOK || (st.st_mode & mask) == 0)
	return;

    if (!setjmp (myctx)) {
	SIGNAL (SIGALRM, alrmser);
	alarm (2);
	td = open (ttyspec, O_WRONLY);
	alarm (0);
	if (td == NOTOK)
	    return;
    } else {
	alarm (0);
	return;
    }

    lseek (md, (off_t) 0, SEEK_SET);

    while ((i = read (md, buffer, sizeof(buffer))) > 0)
	if (write (td, buffer, i) != i)
	    break;

    close (td);
}

