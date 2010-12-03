
/*
 * prompter.c -- simple prompting editor front-end
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/signals.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>

#ifdef HAVE_TERMIOS_H
# include <termios.h>
#else
# ifdef HAVE_TERMIO_H
#  include <termio.h>
# else
#  include <sgtty.h>
# endif
#endif

#define	QUOTE '\\'

#ifndef	CKILL
# define CKILL '@'
#endif

#ifndef	CERASE
# define CERASE '#'
#endif

static struct swit switches[] = {
#define	ERASESW	0
    { "erase chr", 0 },
#define	KILLSW	1
    { "kill chr", 0 },
#define	PREPSW	2
    { "prepend", 0 },
#define	NPREPSW	3
    { "noprepend", 0 },	
#define	RAPDSW	4
    { "rapid", 0 },
#define	NRAPDSW	5
    { "norapid", 0 },
#define	BODYSW	6
    { "body", -4 },
#define	NBODYSW	7
    { "nobody", -6 },
#define	DOTSW	8
    { "doteof", 0 },
#define	NDOTSW	9
    { "nodoteof", 0 },
#define VERSIONSW 10
    { "version", 0 },
#define	HELPSW	11
    { "help", 0 },
    { NULL, 0 }
};


#ifdef HAVE_TERMIOS_H
static struct termios tio;
# define ERASE tio.c_cc[VERASE]
# define KILL  tio.c_cc[VKILL]
# define INTR  tio.c_cc[VINTR]
#else
# ifdef HAVE_TERMIO_H
static struct termio tio;
#  define ERASE tio.c_cc[VERASE]
#  define KILL  tio.c_cc[VKILL]
#  define INTR  tio.c_cc[VINTR]
# else
static struct sgttyb tio;
static struct tchars tc;
#  define ERASE tio.sg_erase
#  define KILL  tio.sg_kill
#  define INTR  tc.t_intrc
# endif
#endif

static int wtuser = 0;
static int sigint = 0;
static jmp_buf sigenv;

/*
 * prototypes
 */
int getln (char *, int);
static int chrcnv (char *);
static void chrdsp (char *, char);
static RETSIGTYPE intrser (int);


int
main (int argc, char **argv)
{
    int body = 1, prepend = 1, rapid = 0;
    int doteof = 0, fdi, fdo, i, state;
    char *cp, *drft = NULL, *erasep = NULL;
    char *killp = NULL, name[NAMESZ], field[BUFSIZ];
    char buffer[BUFSIZ], tmpfil[BUFSIZ];
    char **arguments, **argp;
    FILE *in, *out;
    char *tfile = NULL;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++))
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buffer, sizeof(buffer), "%s [switches] file",
			invo_name);
		    print_help (buffer, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case ERASESW: 
		    if (!(erasep = *argp++) || *erasep == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case KILLSW: 
		    if (!(killp = *argp++) || *killp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case PREPSW: 
		    prepend++;
		    continue;
		case NPREPSW: 
		    prepend = 0;
		    continue;

		case RAPDSW: 
		    rapid++;
		    continue;
		case NRAPDSW: 
		    rapid = 0;
		    continue;

		case BODYSW: 
		    body++;
		    continue;
		case NBODYSW: 
		    body = 0;
		    continue;

		case DOTSW: 
		    doteof++;
		    continue;
		case NDOTSW: 
		    doteof = 0;
		    continue;
	    }
	} else {
	    if (!drft)
		drft = cp;
	}

    if (!drft)
	adios (NULL, "usage: %s [switches] file", invo_name);
    if ((in = fopen (drft, "r")) == NULL)
	adios (drft, "unable to open");

    tfile = m_mktemp2(NULL, invo_name, NULL, &out);
    if (tfile == NULL) adios("prompter", "unable to create temporary file");
    chmod (tmpfil, 0600);
    strncpy (tmpfil, tfile, sizeof(tmpfil));

    /*
     * Are we changing the kill or erase character?
     */
    if (killp || erasep) {
#ifdef HAVE_TERMIOS_H
	cc_t save_erase, save_kill;
#else
	int save_erase, save_kill;
#endif

	/* get the current terminal attributes */
#ifdef HAVE_TERMIOS_H
	tcgetattr(0, &tio);
#else
# ifdef HAVE_TERMIO_H
	ioctl(0, TCGETA, &tio);
# else
	ioctl (0, TIOCGETP, (char *) &tio);
	ioctl (0, TIOCGETC, (char *) &tc);
# endif
#endif

	/* save original kill, erase character for later */
	save_kill = KILL;
	save_erase = ERASE;

	/* set new kill, erase character in terminal structure */
	KILL = killp ? chrcnv (killp) : save_kill;
	ERASE = erasep ? chrcnv (erasep) : save_erase;

	/* set the new terminal attributes */
#ifdef HAVE_TERMIOS_H
	 tcsetattr(0, TCSADRAIN, &tio);
#else
# ifdef HAVE_TERMIO_H
	ioctl(0, TCSETAW, &tio);
# else
	ioctl (0, TIOCSETN, (char *) &tio);
# endif
#endif

	/* print out new kill erase characters */
	chrdsp ("erase", ERASE);
	chrdsp (", kill", KILL);
	chrdsp (", intr", INTR);
	putchar ('\n');
	fflush (stdout);

	/*
	 * We set the kill and erase character back to original
	 * setup in terminal structure so we can easily
	 * restore it upon exit.
	 */
	KILL = save_kill;
	ERASE = save_erase;
    }

    sigint = 0;
    SIGNAL2 (SIGINT, intrser);

    /*
     * Loop through the lines of the draft skeleton.
     */
    for (state = FLD;;) {
	switch (state = m_getfld (state, name, field, sizeof(field), in)) {
	    case FLD: 
	    case FLDEOF: 
	    case FLDPLUS: 
		/*
		 * Check if the value of field contains anything
		 * other than space or tab.
		 */
		for (cp = field; *cp; cp++)
		    if (*cp != ' ' && *cp != '\t')
			break;

		/* If so, just add header line to draft */
		if (*cp++ != '\n' || *cp != 0) {
		    printf ("%s:%s", name, field);
		    fprintf (out, "%s:%s", name, field);
		    while (state == FLDPLUS) {
			state =
			    m_getfld (state, name, field, sizeof(field), in);
			printf ("%s", field);
			fprintf (out, "%s", field);
		    }
		} else {
		    /* Else, get value of header field */
		    printf ("%s: ", name);
		    fflush (stdout);
		    i = getln (field, sizeof(field));
		    if (i == -1) {
abort:
			if (killp || erasep) {
#ifdef HAVE_TERMIOS_H
			    tcsetattr(0, TCSADRAIN, &tio);
#else
# ifdef HAVE_TERMIO
			    ioctl (0, TCSETA, &tio);
# else
			    ioctl (0, TIOCSETN, (char *) &tio);
# endif
#endif
			}
			unlink (tmpfil);
			done (1);
		    }
		    if (i != 0 || (field[0] != '\n' && field[0] != 0)) {
			fprintf (out, "%s:", name);
			do {
			    if (field[0] != ' ' && field[0] != '\t')
				putc (' ', out);
			    fprintf (out, "%s", field);
			} while (i == 1
				    && (i = getln (field, sizeof(field))) >= 0);
			if (i == -1)
			    goto abort;
		    }
		}

		if (state == FLDEOF) {	/* moby hack */
		    fprintf (out, "--------\n");
		    printf ("--------\n");
		    if (!body)
			break;
		    goto no_body;
		}
		continue;

	    case BODY: 
	    case BODYEOF:
	    case FILEEOF: 
	        if (!body)
	            break;
		fprintf (out, "--------\n");
		if (field[0] == 0 || !prepend)
		    printf ("--------\n");
		if (field[0]) {
		    if (prepend && body) {
			printf ("\n--------Enter initial text\n\n");
			fflush (stdout);
			for (;;) {
			    getln (buffer, sizeof(buffer));
			    if (doteof && buffer[0] == '.' && buffer[1] == '\n')
				break;
			    if (buffer[0] == 0)
				break;
			    fprintf (out, "%s", buffer);
			}
		    }

		    do {
			fprintf (out, "%s", field);
			if (!rapid && !sigint)
			    printf ("%s", field);
		    } while (state == BODY &&
			    (state = m_getfld (state, name, field, sizeof(field), in)));
		    if (prepend || !body)
			break;
		    else
			printf ("\n--------Enter additional text\n\n");
		}
no_body:
		fflush (stdout);
		for (;;) {
		    getln (field, sizeof(field));
		    if (doteof && field[0] == '.' && field[1] == '\n')
			break;
		    if (field[0] == 0)
			break;
 		    fprintf (out, "%s", field);
		}
		break;

	    default: 
		adios (NULL, "skeleton is poorly formatted");
	}
	break;
    }

    if (body)
	printf ("--------\n");

    fflush (stdout);
    fclose (in);
    fclose (out);
    SIGNAL (SIGINT, SIG_IGN);

    if (killp || erasep) {
#ifdef HAVE_TERMIOS_H
	 tcsetattr(0, TCSADRAIN, &tio);
#else
# ifdef HAVE_TERMIO_H
	ioctl (0, TCSETAW, &tio);
# else
	ioctl (0, TIOCSETN, (char *) &tio);
# endif
#endif
    }

    if ((fdi = open (tmpfil, O_RDONLY)) == NOTOK)
	adios (tmpfil, "unable to re-open");
    if ((fdo = creat (drft, m_gmprot ())) == NOTOK)
	adios (drft, "unable to write");
    cpydata (fdi, fdo, tmpfil, drft);
    close (fdi);
    close (fdo);
    unlink (tmpfil);

    context_save ();	/* save the context file */
    done (0);
    return 1;
}


int
getln (char *buffer, int n)
{
    int c;
    char *cp;

    cp = buffer;
    *cp = 0;

    switch (setjmp (sigenv)) {
	case OK: 
	    wtuser = 1;
	    break;

	case DONE: 
	    wtuser = 0;
	    return 0;

	default: 
	    wtuser = 0;
	    return NOTOK;
    }

    for (;;) {
	switch (c = getchar ()) {
	    case EOF: 
		clearerr (stdin);
		longjmp (sigenv, DONE);

	    case '\n': 
		if (cp[-1] == QUOTE) {
		    cp[-1] = c;
		    wtuser = 0;
		    return 1;
		}
		*cp++ = c;
		*cp = 0;
		wtuser = 0;
		return 0;

	    default: 
		if (cp < buffer + n)
		    *cp++ = c;
		*cp = 0;
	}
    }
}


static RETSIGTYPE
intrser (int i)
{
#ifndef	RELIABLE_SIGNALS
    SIGNAL (SIGINT, intrser);
#endif

    if (wtuser)
	longjmp (sigenv, NOTOK);
    sigint++;
}


static int
chrcnv (char *cp)
{
    return (*cp != QUOTE ? *cp : m_atoi (++cp));
}


static void
chrdsp (char *s, char c)
{
    printf ("%s ", s);
    if (c < ' ' || c == 0177)
	printf ("^%c", c ^ 0100);
    else
	printf ("%c", c);
}
