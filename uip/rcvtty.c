
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
#include <setjmp.h>
#include <h/rcvmail.h>
#include <h/scansbr.h>
#include <h/tws.h>
#include <h/mts.h>
#include <fcntl.h>

#ifdef HAVE_GETUTXENT
#include <utmpx.h>
#endif /* HAVE_GETUTXENT */

#define	SCANFMT	\
"%2(hour{dtimenow}):%02(min{dtimenow}): %<(size)%5(size) %>%<{encrypted}E%>\
%<(mymbox{from})%<{to}To:%14(friendly{to})%>%>%<(zero)%17(friendly{from})%>  \
%{subject}%<{body}<<%{body}>>%>"

#define RCVTTY_SWITCHES \
    X("biff", 0, BIFFSW) \
    X("form formatfile", 0, FORMSW) \
    X("format string", 5, FMTSW) \
    X("width columns", 0, WIDTHSW) \
    X("newline", 0, NLSW) \
    X("nonewline", 0, NNLSW) \
    X("bell", 0, BELSW) \
    X("nobell", 0, NBELSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(RCVTTY);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(RCVTTY, switches);
#undef X

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
static void alrmser (int);
static int message_fd (char **);
static int header_fd (void);
static void alert (char *, int);


int
main (int argc, char **argv)
{
    int md, vecp = 0;
    char *cp, *user, buf[BUFSIZ], tty[BUFSIZ];
    char **argp, **arguments, *vec[MAXARGS];
    struct utmpx *utp;
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
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

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

#if HAVE_GETUTXENT
    setutxent();
    while ((utp = getutxent()) != NULL) {
        if (utp->ut_type == USER_PROCESS && utp->ut_user[0] != 0
               && utp->ut_line[0] != 0
               && strncmp (user, utp->ut_user, sizeof(utp->ut_user)) == 0) {
            strncpy (tty, utp->ut_line, sizeof(utp->ut_line));
	    alert (tty, md);
        }
    }
    endutxent();
#endif /* HAVE_GETUTXENT */

    exit (RCV_MOK);
}


static void
alrmser (int i)
{
    NMH_UNUSED (i);

    longjmp (myctx, 1);
}


static int
message_fd (char **vec)
{
    pid_t child_id;
    int bytes, fd, seconds;
    char tmpfil[BUFSIZ];
    struct stat st;

    fd = mkstemp (strncpy (tmpfil, "/tmp/rcvttyXXXXX", sizeof(tmpfil)));
    unlink (tmpfil);

    if ((child_id = fork()) == NOTOK) {
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
    scan_finished ();
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

