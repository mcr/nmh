
/*
 * mhlsbr.c -- main routines for nmh message lister
 *
 * $Id$
 */

#include <h/mh.h>
#include <h/signals.h>
#include <h/addrsbr.h>
#include <h/fmt_scan.h>
#include <zotnet/tws/tws.h>
#include <setjmp.h>
#include <signal.h>

/*
 * MAJOR BUG:
 * for a component containing addresses, ADDRFMT, if COMPRESS is also
 * set, then addresses get split wrong (not at the spaces between commas).
 * To fix this correctly, putstr() should know about "atomic" strings that
 * must NOT be broken across lines.  That's too difficult for right now
 * (it turns out that there are a number of degernate cases), so in
 * oneline(), instead of
 *
 *       (*onelp == '\n' && !onelp[1])
 *
 * being a terminating condition,
 *
 *       (*onelp == '\n' && (!onelp[1] || (flags & ADDRFMT)))
 *
 * is used instead.  This cuts the line prematurely, and gives us a much
 * better chance of getting things right.
 */

#define ONECOMP  0
#define TWOCOMP  1
#define	BODYCOMP 2

#define	QUOTE	'\\'

static struct swit mhlswitches[] = {
#define	BELLSW         0
    { "bell", 0 },
#define	NBELLSW        1
    { "nobell", 0 },
#define	CLRSW          2
    { "clear", 0 },
#define	NCLRSW         3
    { "noclear", 0 },
#define	FACESW         4
    { "faceproc program", 0 },
#define	NFACESW        5
    { "nofaceproc", 0 },
#define	FOLDSW         6
    { "folder +folder", 0 },
#define	FORMSW         7
    { "form formfile", 0 },
#define	PROGSW         8
    { "moreproc program", 0 },
#define	NPROGSW        9
    { "nomoreproc", 0 },
#define	LENSW         10
    { "length lines", 0 },
#define	WIDTHSW       11
    { "width columns", 0 },
#define	SLEEPSW       12
    { "sleep seconds",  0 },
#define	BITSTUFFSW    13
    { "dashstuffing", -12 },	/* interface from forw */
#define	NBITSTUFFSW   14
    { "nodashstuffing", -14 },	/* interface from forw */
#define VERSIONSW     15
    { "version", 0 },
#define	HELPSW        16
    { "help", 4 },
#define	FORW1SW       17
    { "forward", -7 },		/* interface from forw */
#define	FORW2SW       18
    { "forwall", -7 },		/* interface from forw */
#define	DGSTSW        19
    { "digest list", -6 },
#define VOLUMSW       20
    { "volume number", -6 },
#define ISSUESW       21
    { "issue number", -5 },
#define NBODYSW       22
    { "nobody", -6 },
    { NULL, 0 }
};

#define NOCOMPONENT 0x000001	/* don't show component name         */
#define UPPERCASE   0x000002	/* display in all upper case         */
#define CENTER      0x000004	/* center line                       */
#define CLEARTEXT   0x000008	/* cleartext                         */
#define EXTRA       0x000010	/* an "extra" component              */
#define HDROUTPUT   0x000020	/* already output                    */
#define CLEARSCR    0x000040	/* clear screen                      */
#define LEFTADJUST  0x000080	/* left justify multiple lines       */
#define COMPRESS    0x000100	/* compress text                     */
#define	ADDRFMT     0x000200	/* contains addresses                */
#define	BELL        0x000400	/* sound bell at EOP                 */
#define	DATEFMT     0x000800	/* contains dates                    */
#define	FORMAT      0x001000	/* parse address/date/RFC-2047 field */
#define	INIT        0x002000	/* initialize component              */
#define	FACEFMT     0x004000	/* contains face                     */
#define	FACEDFLT    0x008000	/* default for face                  */
#define	SPLIT       0x010000	/* split headers (don't concatenate) */
#define	NONEWLINE   0x020000	/* don't write trailing newline      */
#define	LBITS	"\020\01NOCOMPONENT\02UPPERCASE\03CENTER\04CLEARTEXT\05EXTRA\06HDROUTPUT\07CLEARSCR\010LEFTADJUST\011COMPRESS\012ADDRFMT\013BELL\014DATEFMT\015FORMAT\016INIT\017FACEFMT\020FACEDFLT\021SPLIT\022NONEWLINE"
#define	GFLAGS	(NOCOMPONENT | UPPERCASE | CENTER | LEFTADJUST | COMPRESS | SPLIT)

struct mcomp {
    char *c_name;		/* component name          */
    char *c_text;		/* component text          */
    char *c_ovtxt;		/* text overflow indicator */
    char *c_nfs;		/* iff FORMAT              */
    struct format *c_fmt;	/*   ..                    */
    char *c_face;		/* face designator         */
    int c_offset;		/* left margin indentation */
    int c_ovoff;		/* overflow indentation    */
    int c_width;		/* width of field          */
    int c_cwidth;		/* width of component      */
    int c_length;		/* length in lines         */
    long c_flags;
    struct mcomp *c_next;
};

static struct mcomp *msghd = NULL;
static struct mcomp *msgtl = NULL;
static struct mcomp *fmthd = NULL;
static struct mcomp *fmttl = NULL;

static struct mcomp global = {
    NULL, NULL, "", NULL, NULL, 0, -1, 80, -1, 40, BELL, 0
};

static struct mcomp holder = {
    NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, NOCOMPONENT, 0
};

struct pair {
    char *p_name;
    long p_flags;
};

static struct pair pairs[] = {
    { "Date",            DATEFMT },
    { "From",            ADDRFMT|FACEDFLT },
    { "Sender",          ADDRFMT },
    { "Reply-To",        ADDRFMT },
    { "To",              ADDRFMT },
    { "cc",              ADDRFMT },
    { "Bcc",             ADDRFMT },
    { "Resent-Date",     DATEFMT },
    { "Resent-From",     ADDRFMT },
    { "Resent-Sender",   ADDRFMT },
    { "Resent-Reply-To", ADDRFMT },
    { "Resent-To",       ADDRFMT },
    { "Resent-cc",       ADDRFMT },
    { "Resent-Bcc",      ADDRFMT },
    { "Face",            FACEFMT },
    { NULL,              0 }
};

struct triple {
    char *t_name;
    long t_on;
    long t_off;
};

static struct triple triples[] = {
    { "nocomponent",   NOCOMPONENT, 0 },
    { "uppercase",     UPPERCASE,   0 },
    { "nouppercase",   0,           UPPERCASE },
    { "center",        CENTER,      0 },
    { "nocenter",      0,           CENTER },
    { "clearscreen",   CLEARSCR,    0 },
    { "noclearscreen", 0,           CLEARSCR },
    { "noclear",       0,           CLEARSCR },
    { "leftadjust",    LEFTADJUST,  0 },
    { "noleftadjust",  0,           LEFTADJUST },
    { "compress",      COMPRESS,    0 },
    { "nocompress",    0,           COMPRESS },
    { "split",         SPLIT,       0 },
    { "nosplit",       0,           SPLIT },
    { "addrfield",     ADDRFMT,     DATEFMT },
    { "bell",          BELL,        0 },
    { "nobell",        0,           BELL },
    { "datefield",     DATEFMT,     ADDRFMT },
    { "newline",       0,           NONEWLINE },
    { "nonewline",     NONEWLINE,   0 },
    { NULL,            0,           0 }
};


static int bellflg   = 0;
static int clearflg  = 0;
static int dashstuff = 0;
static int dobody    = 1;
static int forwflg   = 0;
static int forwall   = 0;

static int sleepsw = NOTOK;

static char *digest = NULL;
static int volume = 0;
static int issue = 0;

static int exitstat = 0;
static int mhldebug = 0;

#define	PITTY	(-1)
#define	NOTTY	0
#define	ISTTY	1
static int ontty = NOTTY;

static int row;
static int column;

static int lm;
static int llim;
static int ovoff;
static int term;
static int wid;

static char *ovtxt;

static char *onelp;

static char *parptr;

static int num_ignores = 0;
static char *ignores[MAXARGS];

static  jmp_buf env;
static  jmp_buf mhlenv;

static char delim3[] =		/* from forw.c */
    "\n----------------------------------------------------------------------\n\n";
static char delim4[] = "\n------------------------------\n\n";

static FILE *(*mhl_action) () = (FILE *(*) ()) 0;


/*
 * Redefine a couple of functions.
 * These are undefined later in the code.
 */
#define	adios mhladios
#define	done  mhldone

/*
 * prototypes
 */
static void mhl_format (char *, int, int);
static int evalvar (struct mcomp *);
static int ptoi (char *, int *);
static int ptos (char *, char **);
static char *parse (void);
static void process (char *, char *, int, int);
static void mhlfile (FILE *, char *, int, int);
static int mcomp_flags (char *);
static char *mcomp_add (long, char *, char *);
static void mcomp_format (struct mcomp *, struct mcomp *);
static struct mcomp *add_queue (struct mcomp **, struct mcomp **, char *, char *, int);
static void free_queue (struct mcomp **, struct mcomp **);
static void putcomp (struct mcomp *, struct mcomp *, int);
static char *oneline (char *, long);
static void putstr (char *);
static void putch (char);
static RETSIGTYPE intrser (int);
static RETSIGTYPE pipeser (int);
static RETSIGTYPE quitser (int);
static void face_format (struct mcomp *);
static int doface (struct mcomp *);
static void mhladios (char *, char *, ...);
static void mhldone (int);
static void m_popen (char *);

int mhl (int, char **);
int mhlsbr (int, char **, FILE *(*)());
void m_pclose (void);

void clear_screen (void);             /* from termsbr.c */
int SOprintf (char *, ...);           /* from termsbr.c */
int sc_width (void);                  /* from termsbr.c */
int sc_length (void);                 /* from termsbr.c */
int sc_hardcopy (void);               /* from termsbr.c */
struct hostent *gethostbystring ();


int
mhl (int argc, char **argv)
{
    int length = 0, nomore = 0;
    int i, width = 0, vecp = 0;
    char *cp, *folder = NULL, *form = NULL;
    char buf[BUFSIZ], *files[MAXARGS];
    char **argp, **arguments;

    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    if ((cp = getenv ("MHLDEBUG")) && *cp)
	mhldebug++;

    if ((cp = getenv ("FACEPROC")))
	faceproc = cp;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, mhlswitches)) {
		case AMBIGSW: 
		    ambigsw (cp, mhlswitches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown\n", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [switches] [files ...]", invo_name);
		    print_help (buf, mhlswitches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case BELLSW: 
		    bellflg = 1;
		    continue;
		case NBELLSW: 
		    bellflg = -1;
		    continue;

		case CLRSW: 
		    clearflg = 1;
		    continue;
		case NCLRSW: 
		    clearflg = -1;
		    continue;

		case FOLDSW: 
		    if (!(folder = *argp++) || *folder == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case FORMSW: 
		    if (!(form = *argp++) || *form == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case FACESW:
		    if (!(faceproc = *argp++) || *faceproc == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case NFACESW:
		    faceproc = NULL;
		    continue;
		case SLEEPSW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    sleepsw = atoi (cp);/* ZERO ok! */
		    continue;

		case PROGSW:
		    if (!(moreproc = *argp++) || *moreproc == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case NPROGSW:
		    nomore++;
		    continue;

		case LENSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((length = atoi (cp)) < 1)
			adios (NULL, "bad argument %s %s", argp[-2], cp);
		    continue;
		case WIDTHSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((width = atoi (cp)) < 1)
			adios (NULL, "bad argument %s %s", argp[-2], cp);
		    continue;

		case DGSTSW: 
		    if (!(digest = *argp++) || *digest == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case ISSUESW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((issue = atoi (cp)) < 1)
			adios (NULL, "bad argument %s %s", argp[-2], cp);
		    continue;
		case VOLUMSW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((volume = atoi (cp)) < 1)
			adios (NULL, "bad argument %s %s", argp[-2], cp);
		    continue;

		case FORW2SW: 
		    forwall++;	/* fall */
		case FORW1SW: 
		    forwflg++;
		    clearflg = -1;/* XXX */
		    continue;

		case BITSTUFFSW: 
		    dashstuff = 1;	/* trinary logic */
		    continue;
		case NBITSTUFFSW: 
		    dashstuff = -1;	/* trinary logic */
		    continue;

		case NBODYSW: 
		    dobody = 0;
		    continue;
	    }
	}
	files[vecp++] = cp;
    }

    if (!folder)
	folder = getenv ("mhfolder");

    if (isatty (fileno (stdout))) {
	if (!nomore && !sc_hardcopy() && moreproc && *moreproc != '\0') {
	    if (mhl_action) {
		SIGNAL (SIGINT, SIG_IGN);
		SIGNAL2 (SIGQUIT, quitser);
	    }
	    SIGNAL2 (SIGPIPE, pipeser);
	    m_popen (moreproc);
	    ontty = PITTY;
	} else {
	    SIGNAL (SIGINT, SIG_IGN);
	    SIGNAL2 (SIGQUIT, quitser);
	    ontty = ISTTY;
	}
    } else {
	ontty = NOTTY;
    }

    mhl_format (form ? form : mhlformat, length, width);

    if (vecp == 0) {
	process (folder, NULL, 1, vecp = 1);
    } else {
	for (i = 0; i < vecp; i++)
	    process (folder, files[i], i + 1, vecp);
    }

    if (forwall) {
	if (digest) {
	    printf ("%s", delim4);
	    if (volume == 0) {
		snprintf (buf, sizeof(buf), "End of %s Digest\n", digest);
	    } else {
		snprintf (buf, sizeof(buf), "End of %s Digest [Volume %d Issue %d]\n",
			digest, volume, issue);
	    }
	    i = strlen (buf);
	    for (cp = buf + i; i > 1; i--)
		*cp++ = '*';
	    *cp++ = '\n';
	    *cp = 0;
	    printf ("%s", buf);
	}
	else
	    printf ("\n------- End of Forwarded Message%s\n\n",
		    vecp > 1 ? "s" : "");
    }

    if (clearflg > 0 && ontty == NOTTY)
	clear_screen ();

    if (ontty == PITTY)
	m_pclose ();

    return exitstat;
}


static void
mhl_format (char *file, int length, int width)
{
    int i;
    char *bp, *cp, **ip;
    char *ap, buffer[BUFSIZ], name[NAMESZ];
    struct mcomp *c1;
    struct stat st;
    FILE *fp;
    static dev_t dev = 0;
    static ino_t ino = 0;
    static time_t mtime = 0;

    if (fmthd != NULL) {
	if (stat (etcpath (file), &st) != NOTOK
		&& mtime == st.st_mtime
		&& dev == st.st_dev
		&& ino == st.st_ino)
	    goto out;
	else
	    free_queue (&fmthd, &fmttl);
    }

    if ((fp = fopen (etcpath (file), "r")) == NULL)
	adios (file, "unable to open format file");

    if (fstat (fileno (fp), &st) != NOTOK) {
	mtime = st.st_mtime;
	dev = st.st_dev;
	ino = st.st_ino;
    }

    global.c_ovtxt = global.c_nfs = NULL;
    global.c_fmt = NULL;
    global.c_offset = 0;
    global.c_ovoff = -1;
    if ((i = sc_width ()) > 5)
	global.c_width = i;
    global.c_cwidth = -1;
    if ((i = sc_length ()) > 5)
	global.c_length = i - 1;
    global.c_flags = BELL;		/* BELL is default */
    *(ip = ignores) = NULL;

    while (vfgets (fp, &ap) == OK) {
	bp = ap;
	if (*bp == ';')
	    continue;

	if ((cp = strchr(bp, '\n')))
	    *cp = 0;

	if (*bp == ':') {
	    c1 = add_queue (&fmthd, &fmttl, NULL, bp + 1, CLEARTEXT);
	    continue;
	}

	parptr = bp;
	strncpy (name, parse(), sizeof(name));
	switch (*parptr) {
	    case '\0': 
	    case ',': 
	    case '=': 
		/*
		 * Split this list of fields to ignore, and copy
		 * it to the end of the current "ignores" list.
		 */
		if (!strcasecmp (name, "ignores")) {
		    char **tmparray, **p;
		    int n = 0;

		    /* split the fields */
		    tmparray = brkstring (getcpy (++parptr), ",", NULL);

		    /* count number of fields split */
		    p = tmparray;
		    while (*p++)
			n++;

		    /* copy pointers to split fields to ignores array */
		    ip = copyip (tmparray, ip, MAXARGS - num_ignores);
		    num_ignores += n;
		    continue;
		}
		parptr = bp;
		while (*parptr) {
		    if (evalvar (&global))
			adios (NULL, "format file syntax error: %s", bp);
		    if (*parptr)
			parptr++;
		}
		continue;

	    case ':': 
		c1 = add_queue (&fmthd, &fmttl, name, NULL, INIT);
		while (*parptr == ':' || *parptr == ',') {
		    parptr++;
		    if (evalvar (c1))
			adios (NULL, "format file syntax error: %s", bp);
		}
		if (!c1->c_nfs && global.c_nfs) {
		    if (c1->c_flags & DATEFMT) {
			if (global.c_flags & DATEFMT)
			    c1->c_nfs = getcpy (global.c_nfs);
		    }
		    else
			if (c1->c_flags & ADDRFMT) {
			    if (global.c_flags & ADDRFMT)
				c1->c_nfs = getcpy (global.c_nfs);
			}
		}
		continue;

	    default: 
		adios (NULL, "format file syntax error: %s", bp);
	}
    }
    fclose (fp);

    if (mhldebug) {
	for (c1 = fmthd; c1; c1 = c1->c_next) {
	    fprintf (stderr, "c1: name=\"%s\" text=\"%s\" ovtxt=\"%s\"\n",
		    c1->c_name, c1->c_text, c1->c_ovtxt);
	    fprintf (stderr, "\tnfs=0x%x fmt=0x%x\n",
		    (unsigned int) c1->c_nfs, (unsigned int) c1->c_fmt);
	    fprintf (stderr, "\toffset=%d ovoff=%d width=%d cwidth=%d length=%d\n",
		    c1->c_offset, c1->c_ovoff, c1->c_width,
		    c1->c_cwidth, c1->c_length);
	    fprintf (stderr, "\tflags=%s\n",
		    snprintb (buffer, sizeof(buffer), (unsigned) c1->c_flags, LBITS));
	}
    }

out:
    if (clearflg == 1) {
	global.c_flags |= CLEARSCR;
    } else {
	if (clearflg == -1)
	    global.c_flags &= ~CLEARSCR;
    }

    switch (bellflg) {		/* command line may override format file */
	case 1: 
	    global.c_flags |= BELL;
	    break;
	case -1: 
	    global.c_flags &= ~BELL;
	    break;
    }

    if (length)
	global.c_length = length;
    if (width)
	global.c_width = width;
    if (global.c_length < 5)
	global.c_length = 10000;
    if (global.c_width < 5)
	global.c_width = 10000;
}


static int
evalvar (struct mcomp *c1)
{
    char *cp, name[NAMESZ];
    struct triple *ap;

    if (!*parptr)
	return 0;
    strncpy (name, parse(), sizeof(name));

    if (!strcasecmp (name, "component")) {
	if (ptos (name, &c1->c_text))
	    return 1;
	c1->c_flags &= ~NOCOMPONENT;
	return 0;
    }

    if (!strcasecmp (name, "overflowtext"))
	return ptos (name, &c1->c_ovtxt);

    if (!strcasecmp (name, "formatfield")) {
	char *nfs;

	if (ptos (name, &cp))
	    return 1;
	nfs = new_fs (NULL, NULL, cp);
	c1->c_nfs = getcpy (nfs);
	c1->c_flags |= FORMAT;
	return 0;
    }

    if (!strcasecmp (name, "decode")) {
	char *nfs;

	nfs = new_fs (NULL, NULL, "%(decode{text})");
	c1->c_nfs = getcpy (nfs);
	c1->c_flags |= FORMAT;
	return 0;
    }

    if (!strcasecmp (name, "offset"))
	return ptoi (name, &c1->c_offset);
    if (!strcasecmp (name, "overflowoffset"))
	return ptoi (name, &c1->c_ovoff);
    if (!strcasecmp (name, "width"))
	return ptoi (name, &c1->c_width);
    if (!strcasecmp (name, "compwidth"))
	return ptoi (name, &c1->c_cwidth);
    if (!strcasecmp (name, "length"))
	return ptoi (name, &c1->c_length);
    if (!strcasecmp (name, "nodashstuffing"))
	return (dashstuff = -1);

    for (ap = triples; ap->t_name; ap++)
	if (!strcasecmp (ap->t_name, name)) {
	    c1->c_flags |= ap->t_on;
	    c1->c_flags &= ~ap->t_off;
	    return 0;
	}

    return 1;
}


static int
ptoi (char *name, int *i)
{
    char *cp;

    if (*parptr++ != '=' || !*(cp = parse ())) {
	advise (NULL, "missing argument to variable %s", name);
	return 1;
    }

    *i = atoi (cp);
    return 0;
}


static int
ptos (char *name, char **s)
{
    char c, *cp;

    if (*parptr++ != '=') {
	advise (NULL, "missing argument to variable %s", name);
	return 1;
    }

    if (*parptr != '"') {
	for (cp = parptr;
		*parptr && *parptr != ':' && *parptr != ',';
		parptr++)
	    continue;
    } else {
	for (cp = ++parptr; *parptr && *parptr != '"'; parptr++)
	    if (*parptr == QUOTE)
		if (!*++parptr)
		    parptr--;
    }
    c = *parptr;
    *parptr = 0;
    *s = getcpy (cp);
    if ((*parptr = c) == '"')
	parptr++;
    return 0;
}


static char *
parse (void)
{
    int c;
    char *cp;
    static char result[NAMESZ];

    for (cp = result; *parptr && (cp - result < NAMESZ); parptr++) {
	c = *parptr;
	if (isalnum (c)
		|| c == '.'
		|| c == '-'
		|| c == '_'
		|| c =='['
		|| c == ']')
	    *cp++ = c;
	else
	    break;
    }
    *cp = '\0';

    return result;
}


static void
process (char *folder, char *fname, int ofilen, int ofilec)
{
    char *cp;
    FILE *fp;
    struct mcomp *c1;

    switch (setjmp (env)) {
	case OK: 
	    if (fname) {
		fp = mhl_action ? (*mhl_action) (fname) : fopen (fname, "r");
		if (fp == NULL) {
		    advise (fname, "unable to open");
		    exitstat++;
		    return;
		}
	    } else {
		fname = "(stdin)";
		fp = stdin;
	    }
	    cp = folder ? concat (folder, ":", fname, NULL) : getcpy (fname);
	    if (ontty != PITTY)
		SIGNAL (SIGINT, intrser);
	    mhlfile (fp, cp, ofilen, ofilec);  /* FALL THROUGH! */

	default: 
	    if (ontty != PITTY)
		SIGNAL (SIGINT, SIG_IGN);
	    if (mhl_action == NULL && fp != stdin)
		fclose (fp);
	    free (cp);
	    if (holder.c_text) {
		free (holder.c_text);
		holder.c_text = NULL;
	    }
	    free_queue (&msghd, &msgtl);
	    for (c1 = fmthd; c1; c1 = c1->c_next)
		c1->c_flags &= ~HDROUTPUT;
	    break;
    }
}


static void
mhlfile (FILE *fp, char *mname, int ofilen, int ofilec)
{
    int state;
    struct mcomp *c1, *c2, *c3;
    char **ip, name[NAMESZ], buf[BUFSIZ];

    if (forwall) {
	if (digest)
	    printf ("%s", ofilen == 1 ? delim3 : delim4);
	else {
	    printf ("\n-------");
	    if (ofilen == 1)
		printf (" Forwarded Message%s", ofilec > 1 ? "s" : "");
	    else
		printf (" Message %d", ofilen);
	    printf ("\n\n");
	}
    } else {
	switch (ontty) {
	    case PITTY: 
		if (ofilec > 1) {
		    if (ofilen > 1) {
			if ((global.c_flags & CLEARSCR))
			    clear_screen ();
			else
			    printf ("\n\n\n");
		    }
		    printf (">>> %s\n\n", mname);
		}
		break;

	    case ISTTY: 
		strncpy (buf, "\n", sizeof(buf));
		if (ofilec > 1) {
		    if (SOprintf ("Press <return> to list \"%s\"...", mname)) {
			if (ofilen > 1)
			    printf ("\n\n\n");
			printf ("Press <return> to list \"%s\"...", mname);
		    }
		    fflush (stdout);
		    buf[0] = 0;
		    read (fileno (stdout), buf, sizeof(buf));
		}
		if (strchr(buf, '\n')) {
		    if ((global.c_flags & CLEARSCR))
			clear_screen ();
		}
		else
		    printf ("\n");
		break;

	    default: 
		if (ofilec > 1) {
		    if (ofilen > 1) {
			printf ("\n\n\n");
			if (clearflg > 0)
			    clear_screen ();
		    }
		    printf (">>> %s\n\n", mname);
		}
		break;
	}
    }

    for (state = FLD;;) {
	switch (state = m_getfld (state, name, buf, sizeof(buf), fp)) {
	    case FLD: 
	    case FLDPLUS: 
		for (ip = ignores; *ip; ip++)
		    if (!strcasecmp (name, *ip)) {
			while (state == FLDPLUS)
			    state = m_getfld (state, name, buf, sizeof(buf), fp);
			break;
		    }
		if (*ip)
		    continue;

		for (c2 = fmthd; c2; c2 = c2->c_next)
		    if (!strcasecmp (c2->c_name, name))
			break;
		c1 = NULL;
		if (!((c3 = c2 ? c2 : &global)->c_flags & SPLIT))
		    for (c1 = msghd; c1; c1 = c1->c_next)
			if (!strcasecmp (c1->c_name, c3->c_name)) {
			    c1->c_text =
				mcomp_add (c1->c_flags, buf, c1->c_text);
			    break;
			}
		if (c1 == NULL)
		    c1 = add_queue (&msghd, &msgtl, name, buf, 0);
		while (state == FLDPLUS) {
		    state = m_getfld (state, name, buf, sizeof(buf), fp);
		    c1->c_text = add (buf, c1->c_text);
		}
		if (c2 == NULL)
		    c1->c_flags |= EXTRA;
		continue;

	    case BODY: 
	    case FILEEOF: 
		row = column = 0;
		for (c1 = fmthd; c1; c1 = c1->c_next) {
		    if (c1->c_flags & CLEARTEXT) {
			putcomp (c1, c1, ONECOMP);
			continue;
		    }
		    if (!strcasecmp (c1->c_name, "messagename")) {
			holder.c_text = concat ("(Message ", mname, ")\n",
					    NULL);
			putcomp (c1, &holder, ONECOMP);
			free (holder.c_text);
			holder.c_text = NULL;
			continue;
		    }
		    if (!strcasecmp (c1->c_name, "extras")) {
			for (c2 = msghd; c2; c2 = c2->c_next)
			    if (c2->c_flags & EXTRA)
				putcomp (c1, c2, TWOCOMP);
			continue;
		    }
		    if (dobody && !strcasecmp (c1->c_name, "body")) {
			if ((holder.c_text = malloc (sizeof(buf))) == NULL)
			    adios (NULL, "unable to allocate buffer memory");
			strncpy (holder.c_text, buf, sizeof(buf));
			while (state == BODY) {
			    putcomp (c1, &holder, BODYCOMP);
			    state = m_getfld (state, name, holder.c_text,
					sizeof(buf), fp);
			}
			free (holder.c_text);
			holder.c_text = NULL;
			continue;
		    }
		    for (c2 = msghd; c2; c2 = c2->c_next)
			if (!strcasecmp (c2->c_name, c1->c_name)) {
			    putcomp (c1, c2, ONECOMP);
			    if (!(c1->c_flags & SPLIT))
				break;
			}
		    if (faceproc && c2 == NULL && (c1->c_flags & FACEFMT))
			for (c2 = msghd; c2; c2 = c2->c_next)
			    if (c2->c_flags & FACEDFLT) {
				if (c2->c_face == NULL)
				    face_format (c2);
				if ((holder.c_text = c2->c_face)) {
				    putcomp (c1, &holder, ONECOMP);
				    holder.c_text = NULL;
				}
				break;
			    }
		}
		return;

	    case LENERR: 
	    case FMTERR: 
		advise (NULL, "format error in message %s", mname);
		exitstat++;
		return;

	    default: 
		adios (NULL, "getfld() returned %d", state);
	}
    }
}


static int
mcomp_flags (char *name)
{
    struct pair *ap;

    for (ap = pairs; ap->p_name; ap++)
	if (!strcasecmp (ap->p_name, name))
	    return (ap->p_flags);

    return 0;
}


static char *
mcomp_add (long flags, char *s1, char *s2)
{
    char *dp;

    if (!(flags & ADDRFMT))
	return add (s1, s2);

    if (s2 && *(dp = s2 + strlen (s2) - 1) == '\n')
	*dp = 0;

    return add (s1, add (",\n", s2));
}


struct pqpair {
    char *pq_text;
    char *pq_error;
    struct pqpair *pq_next;
};


static void
mcomp_format (struct mcomp *c1, struct mcomp *c2)
{
    int dat[5];
    char *ap, *cp;
    char buffer[BUFSIZ], error[BUFSIZ];
    struct comp *cptr;
    struct pqpair *p, *q;
    struct pqpair pq;
    struct mailname *mp;

    ap = c2->c_text;
    c2->c_text = NULL;
    dat[0] = 0;
    dat[1] = 0;
    dat[2] = 0;
    dat[3] = sizeof(buffer) - 1;
    dat[4] = 0;
    fmt_compile (c1->c_nfs, &c1->c_fmt);

    if (!(c1->c_flags & ADDRFMT)) {
	FINDCOMP (cptr, "text");
	if (cptr)
	    cptr->c_text = ap;
	if ((cp = strrchr(ap, '\n')))	/* drop ending newline */
	    if (!cp[1])
		*cp = 0;

	fmt_scan (c1->c_fmt, buffer, sizeof(buffer) - 1, dat);
	/* Don't need to append a newline, dctime() already did */
	c2->c_text = getcpy (buffer);

	free (ap);
	return;
    }

    (q = &pq)->pq_next = NULL;
    while ((cp = getname (ap))) {
	if ((p = (struct pqpair *) calloc ((size_t) 1, sizeof(*p))) == NULL)
	    adios (NULL, "unable to allocate pqpair memory");

	if ((mp = getm (cp, NULL, 0, AD_NAME, error)) == NULL) {
	    p->pq_text = getcpy (cp);
	    p->pq_error = getcpy (error);
	} else {
	    if ((c1->c_flags & FACEDFLT) && c2->c_face == NULL) {
		char   *h, *o;
		if ((h = mp->m_host) == NULL)
		    h = LocalName ();
		if ((o = OfficialName (h)))
		    h = o;
		c2->c_face = concat ("address ", h, " ", mp->m_mbox,
				    NULL);
	    }
	    p->pq_text = getcpy (mp->m_text);
	    mnfree (mp);
	}
	q = (q->pq_next = p);
    }

    for (p = pq.pq_next; p; p = q) {
	FINDCOMP (cptr, "text");
	if (cptr)
	    cptr->c_text = p->pq_text;
	FINDCOMP (cptr, "error");
	if (cptr)
	    cptr->c_text = p->pq_error;

	fmt_scan (c1->c_fmt, buffer, sizeof(buffer) - 1, dat);
	if (*buffer) {
	    if (c2->c_text)
		c2->c_text = add (",\n", c2->c_text);
	    if (*(cp = buffer + strlen (buffer) - 1) == '\n')
		*cp = 0;
	    c2->c_text = add (buffer, c2->c_text);
	}

	free (p->pq_text);
	if (p->pq_error)
	    free (p->pq_error);
	q = p->pq_next;
	free ((char *) p);
    }

    c2->c_text = add ("\n", c2->c_text);
    free (ap);
}


static struct mcomp *
add_queue (struct mcomp **head, struct mcomp **tail, char *name, char *text, int flags)
{
    struct mcomp *c1;

    if ((c1 = (struct mcomp *) calloc ((size_t) 1, sizeof(*c1))) == NULL)
	adios (NULL, "unable to allocate comp memory");

    c1->c_flags = flags & ~INIT;
    if ((c1->c_name = name ? getcpy (name) : NULL))
	c1->c_flags |= mcomp_flags (c1->c_name);
    c1->c_text = text ? getcpy (text) : NULL;
    if (flags & INIT) {
	if (global.c_ovtxt)
	    c1->c_ovtxt = getcpy (global.c_ovtxt);
	c1->c_offset = global.c_offset;
	c1->c_ovoff = global. c_ovoff;
	c1->c_width = c1->c_length = 0;
	c1->c_cwidth = global.c_cwidth;
	c1->c_flags |= global.c_flags & GFLAGS;
    }
    if (*head == NULL)
	*head = c1;
    if (*tail != NULL)
	(*tail)->c_next = c1;
    *tail = c1;

    return c1;
}


static void
free_queue (struct mcomp **head, struct mcomp **tail)
{
    struct mcomp *c1, *c2;

    for (c1 = *head; c1; c1 = c2) {
	c2 = c1->c_next;
	if (c1->c_name)
	    free (c1->c_name);
	if (c1->c_text)
	    free (c1->c_text);
	if (c1->c_ovtxt)
	    free (c1->c_ovtxt);
	if (c1->c_nfs)
	    free (c1->c_nfs);
	if (c1->c_fmt)
	    free ((char *) c1->c_fmt);
	if (c1->c_face)
	    free (c1->c_face);
	free ((char *) c1);
    }

    *head = *tail = NULL;
}


static void
putcomp (struct mcomp *c1, struct mcomp *c2, int flag)
{
    int count, cchdr;
    char *cp;

    cchdr = 0;
    lm = 0;
    llim = c1->c_length ? c1->c_length : -1;
    wid = c1->c_width ? c1->c_width : global.c_width;
    ovoff = (c1->c_ovoff >= 0 ? c1->c_ovoff : global.c_ovoff)
	+ c1->c_offset;
    if ((ovtxt = c1->c_ovtxt ? c1->c_ovtxt : global.c_ovtxt) == NULL)
	ovtxt = "";
    if (wid < ovoff + strlen (ovtxt) + 5)
	adios (NULL, "component: %s width(%d) too small for overflow(%d)",
		c1->c_name, wid, ovoff + strlen (ovtxt) + 5);
    onelp = NULL;

    if (c1->c_flags & CLEARTEXT) {
	putstr (c1->c_text);
	putstr ("\n");
	return;
    }

    if (c1->c_flags & FACEFMT)
	switch (doface (c2)) {
	    case NOTOK:		/* error */
	    case OK:		/* async faceproc */
		return;

	    default:		/* sync faceproc */
		break;
	}

    if (c1->c_nfs && (c1->c_flags & (ADDRFMT | DATEFMT | FORMAT)))
	mcomp_format (c1, c2);

    if (c1->c_flags & CENTER) {
	count = (c1->c_width ? c1->c_width : global.c_width)
	    - c1->c_offset - strlen (c2->c_text);
	if (!(c1->c_flags & HDROUTPUT) && !(c1->c_flags & NOCOMPONENT))
	    count -= strlen (c1->c_text ? c1->c_text : c1->c_name) + 2;
	lm = c1->c_offset + (count / 2);
    } else {
	if (c1->c_offset)
	    lm = c1->c_offset;
    }

    if (!(c1->c_flags & HDROUTPUT) && !(c1->c_flags & NOCOMPONENT)) {
        if (c1->c_flags & UPPERCASE)		/* uppercase component also */
	    for (cp = (c1->c_text ? c1->c_text : c1->c_name); *cp; cp++)
	        if (islower (*cp))
		    *cp = toupper (*cp);
	putstr (c1->c_text ? c1->c_text : c1->c_name);
	if (flag != BODYCOMP) {
	    putstr (": ");
	    if (!(c1->c_flags & SPLIT))
		c1->c_flags |= HDROUTPUT;

	cchdr++;
	if ((count = c1->c_cwidth -
		strlen (c1->c_text ? c1->c_text : c1->c_name) - 2) > 0)
	    while (count--)
		putstr (" ");
	}
	else
	    c1->c_flags |= HDROUTPUT;		/* for BODYCOMP */
    }

    if (flag == TWOCOMP
	    && !(c2->c_flags & HDROUTPUT)
	    && !(c2->c_flags & NOCOMPONENT)) {
        if (c1->c_flags & UPPERCASE)
	    for (cp = c2->c_name; *cp; cp++)
	        if (islower (*cp))
		    *cp = toupper (*cp);
	putstr (c2->c_name);
	putstr (": ");
	if (!(c1->c_flags & SPLIT))
	    c2->c_flags |= HDROUTPUT;

	cchdr++;
	if ((count = c1->c_cwidth - strlen (c2->c_name) - 2) > 0)
	    while (count--)
		putstr (" ");
    }
    if (c1->c_flags & UPPERCASE)
	for (cp = c2->c_text; *cp; cp++)
	    if (islower (*cp))
		*cp = toupper (*cp);

    count = 0;
    if (cchdr) {
	if (flag == TWOCOMP)
	    count = (c1->c_cwidth >= 0) ? c1->c_cwidth
			: strlen (c2->c_name) + 2;
	else
	    count = (c1->c_cwidth >= 0) ? c1->c_cwidth
			: strlen (c1->c_text ? c1->c_text : c1->c_name) + 2;
    }
    count += c1->c_offset;

    if ((cp = oneline (c2->c_text, c1->c_flags)))
       putstr(cp);
    if (term == '\n')
	putstr ("\n");
    while ((cp = oneline (c2->c_text, c1->c_flags))) {
	lm = count;
	if (flag == BODYCOMP
		&& !(c1->c_flags & NOCOMPONENT))
	    putstr (c1->c_text ? c1->c_text : c1->c_name);
	if (*cp)
	    putstr (cp);
	if (term == '\n')
	    putstr ("\n");
    }
}


static char *
oneline (char *stuff, long flags)
{
    int spc;
    char *cp, *ret;

    if (onelp == NULL)
	onelp = stuff;
    if (*onelp == 0)
	return (onelp = NULL);

    ret = onelp;
    term = 0;
    if (flags & COMPRESS) {
	for (spc = 1, cp = ret; *onelp; onelp++)
	    if (isspace (*onelp)) {
		if (*onelp == '\n' && (!onelp[1] || (flags & ADDRFMT))) {
		    term = '\n';
		    *onelp++ = 0;
		    break;
		}
		else
		    if (!spc) {
			*cp++ = ' ';
			spc++;
		    }
	    }
	    else {
		*cp++ = *onelp;
		spc = 0;
	    }

	*cp = 0;
    }
    else {
	while (*onelp && *onelp != '\n')
	    onelp++;
	if (*onelp == '\n') {
	    term = '\n';
	    *onelp++ = 0;
	}
	if (flags & LEFTADJUST)
	    while (*ret == ' ' || *ret == '\t')
		ret++;
    }
    if (*onelp == 0 && term == '\n' && (flags & NONEWLINE))
	term = 0;

    return ret;
}


static void
putstr (char *string)
{
    if (!column && lm > 0) {
	while (lm > 0)
	    if (lm >= 8) {
		putch ('\t');
		lm -= 8;
	    }
	    else {
		putch (' ');
		lm--;
	    }
    }
    lm = 0;
    while (*string)
	putch (*string++);
}


static void
putch (char ch)
{
    char buf[BUFSIZ];

    if (llim == 0)
	return;

    switch (ch) {
	case '\n': 
	    if (llim > 0)
		llim--;
	    column = 0;
	    row++;
	    if (ontty != ISTTY || row != global.c_length)
		break;
	    if (global.c_flags & BELL)
		putchar ('\007');
	    fflush (stdout);
	    buf[0] = 0;
	    read (fileno (stdout), buf, sizeof(buf));
	    if (strchr(buf, '\n')) {
		if (global.c_flags & CLEARSCR)
		    clear_screen ();
		row = 0;
	    } else {
		putchar ('\n');
		row = global.c_length / 3;
	    }
	    return;

	case '\t': 
	    column |= 07;
	    column++;
	    break;

	case '\b': 
	    column--;
	    break;

	case '\r': 
	    column = 0;
	    break;

	default: 
	    /*
	     * If we are forwarding this message, and the first
	     * column contains a dash, then add a dash and a space.
	     */
	    if (column == 0 && forwflg && (dashstuff >= 0) && ch == '-') {
		putchar ('-');
		putchar (' ');
	    }
	    if (ch >= ' ')
		column++;
	    break;
    }

    if (column >= wid) {
	putch ('\n');
	if (ovoff > 0)
	    lm = ovoff;
	putstr (ovtxt ? ovtxt : "");
	putch (ch);
	return;
    }

    putchar (ch);
}


static RETSIGTYPE
intrser (int i)
{
#ifndef RELIABLE_SIGNALS
    SIGNAL (SIGINT, intrser);
#endif

    discard (stdout);
    putchar ('\n');
    longjmp (env, DONE);
}


static RETSIGTYPE
pipeser (int i)
{
#ifndef RELIABLE_SIGNALS
    SIGNAL (SIGPIPE, pipeser);
#endif

    done (NOTOK);
}


static RETSIGTYPE
quitser (int i)
{
#ifndef RELIABLE_SIGNALS
    SIGNAL (SIGQUIT, quitser);
#endif

    putchar ('\n');
    fflush (stdout);
    done (NOTOK);
}


static void
face_format (struct mcomp *c1)
{
    char *cp;
    struct mailname *mp;

    if ((cp = c1->c_text) == NULL)
	return;

    if ((cp = getname (cp))) {
	if ((mp = getm (cp, NULL, 0, AD_NAME, NULL))) {
	    char *h, *o;
	    if ((h = mp->m_host) == NULL)
		h = LocalName ();
	    if ((o = OfficialName (h)))
		h = o;
	    c1->c_face = concat ("address ", h, " ", mp->m_mbox, NULL);
	}

	while ((cp = getname (cp)))
	    continue;
    }
}


/*
 * faceproc is two elements defining the image agent's location:
 *     Internet host
 *     UDP port
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

static int
doface (struct mcomp *c1)
{
    int	result, sd;
    struct sockaddr_in in_socket;
    struct sockaddr_in *isock = &in_socket;
    static int inited = OK;
    static int addrlen;
    static struct in_addr addr;
    static unsigned short portno;

    if (inited == OK) {
	char *cp;
	char **ap = brkstring (cp = getcpy (faceproc), " ", "\n");
	struct hostent *hp;

	if (ap[0] == NULL || ap[1] == NULL) {
bad_faceproc: ;
	    free (cp);
	    return (inited = NOTOK);
	}

	if (!(hp = gethostbystring (ap[0])))
	    goto bad_faceproc;
	memcpy((char *) &addr, hp->h_addr, addrlen = hp->h_length);

	portno = htons ((unsigned short) atoi (ap[1]));
	free (cp);

	inited = DONE;
    }
    if (inited == NOTOK)
	return NOTOK;

    isock->sin_family = AF_INET;
    isock->sin_port = portno;
    memcpy((char *) &isock->sin_addr, (char *) &addr, addrlen);

    if ((sd = socket (AF_INET, SOCK_DGRAM, 0)) == NOTOK)
	return NOTOK;

    result = sendto (sd, c1->c_text, strlen (c1->c_text), 0,
		(struct sockaddr *) isock, sizeof(*isock));

    close (sd);

    return (result != NOTOK ? OK : NOTOK);
}

/*
 * COMMENTED OUT
 * This version doesn't use sockets
 */
#if 0

static int
doface (struct mcomp *c1)
{
    int i, len, vecp;
    pid_t child_id;
    int result, pdi[2], pdo[2];
    char *bp, *cp;
    char buffer[BUFSIZ], *vec[10];

    if (pipe (pdi) == NOTOK)
	return NOTOK;
    if (pipe (pdo) == NOTOK) {
	close (pdi[0]);
	close (pdi[1]);
	return NOTOK;
    }

    for (i = 0; (child_id = vfork()) == NOTOK && i < 5; i++)
	sleep (5);

    switch (child_id) {
	case NOTOK: 
	    /* oops... fork error */
	    return NOTOK;

	case OK: 
	    /* child process */
	    SIGNAL (SIGINT, SIG_IGN);
	    SIGNAL (SIGQUIT, SIG_IGN);
	    if (pdi[0] != fileno (stdin)) {
		dup2 (pdi[0], fileno (stdin));
		close (pdi[0]);
	    }
	    close (pdi[1]);
	    close (pdo[0]);
	    if (pdo[1] != fileno (stdout)) {
		dup2 (pdo[1], fileno (stdout));
		close (pdo[1]);
	    }
	    vecp = 0;
	    vec[vecp++] = r1bindex (faceproc, '/');
	    vec[vecp++] = "-e";
	    if (sleepsw != NOTOK) {
		vec[vecp++] = "-s";
		snprintf (buffer, sizeof(buffer), "%d", sleepsw);
		vec[vecp++] = buffer;
	    }
	    vec[vecp] = NULL;
	    execvp (faceproc, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (faceproc);
	    _exit (-1);		/* NOTREACHED */

	default: 
	    /* parent process */
	    close (pdi[0]);
	    i = strlen (c1->c_text);
	    if (write (pdi[1], c1->c_text, i) != i)
		adios ("pipe", "error writing to");
	    free (c1->c_text), c1->c_text = NULL;
	    close (pdi[1]);

	    close (pdo[1]);
	    cp = NULL, len = 0;
	    result = DONE;
	    while ((i = read (pdo[0], buffer, strlen (buffer))) > 0) {
		if (cp) {
		    int j;
		    char *dp;
		    if ((dp = realloc (cp, (unsigned) (j = len + i))) == NULL)
			adios (NULL, "unable to allocate face storage");
		    memcpy(dp + len, buffer, i);
		    cp = dp, len = j;
		}
		else {
		    if ((cp = malloc ((unsigned) i)) == NULL)
			adios (NULL, "unable to allocate face storage");
		    memcpy(cp, buffer, i);
		    len = i;
		}
		if (result == DONE)
		    for (bp = buffer + i - 1; bp >= buffer; bp--)
			if (!isascii (*bp) || iscntrl (*bp)) {
			    result = OK;
			    break;
			}
	    }
	    close (pdo[0]);

/* no waiting for child... */

	    if (result == OK) {	/* binary */
		if (write (1, cp, len) != len)
		    adios ("writing", "error");
		free (cp);
	    }
	    else		/* empty */
		if ((c1->c_text = cp) == NULL)
		    result = OK;
	    break;
    }

    return result;
}
#endif /* COMMENTED OUT */


int
mhlsbr (int argc, char **argv, FILE *(*action)())
{
    SIGNAL_HANDLER istat, pstat, qstat;
    char *cp;
    struct mcomp *c1;

    switch (setjmp (mhlenv)) {
	case OK: 
	    cp = invo_name;
	    sleepsw = 0;	/* XXX */
	    bellflg = clearflg = forwflg = forwall = exitstat = 0;
	    digest = NULL;
	    ontty = NOTTY;
	    mhl_action = action;

	    /*
	     * If signal is at default action, then start ignoring
	     * it, else let it set to its current action.
	     */
	    if ((istat = SIGNAL (SIGINT, SIG_IGN)) != SIG_DFL)
		SIGNAL (SIGINT, istat);
	    if ((qstat = SIGNAL (SIGQUIT, SIG_IGN)) != SIG_DFL)
		SIGNAL (SIGQUIT, qstat);
	    pstat = SIGNAL (SIGPIPE, pipeser);
	    mhl (argc, argv);                  /* FALL THROUGH! */

	default: 
	    SIGNAL (SIGINT, istat);
	    SIGNAL (SIGQUIT, qstat);
	    SIGNAL (SIGPIPE, SIG_IGN);/* should probably change to block instead */
	    if (ontty == PITTY)
		m_pclose ();
	    SIGNAL (SIGPIPE, pstat);
	    invo_name = cp;
	    if (holder.c_text) {
		free (holder.c_text);
		holder.c_text = NULL;
	    }
	    free_queue (&msghd, &msgtl);
	    for (c1 = fmthd; c1; c1 = c1->c_next)
		c1->c_flags &= ~HDROUTPUT;
	    return exitstat;
    }
}

#undef adios
#undef done

static void
mhladios (char *what, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    advertise (what, NULL, fmt, ap);
    va_end(ap);
    mhldone (1);
}


static void
mhldone (int status)
{
    exitstat = status;
    if (mhl_action)
	longjmp (mhlenv, DONE);
    else
	done (exitstat);
}


static	int m_pid = NOTOK;
static  int sd = NOTOK;

static void
m_popen (char *name)
{
    int pd[2];

    if (mhl_action && (sd = dup (fileno (stdout))) == NOTOK)
	adios ("standard output", "unable to dup()");

    if (pipe (pd) == NOTOK)
	adios ("pipe", "unable to");

    switch (m_pid = vfork ()) {
	case NOTOK: 
	    adios ("fork", "unable to");

	case OK: 
	    SIGNAL (SIGINT, SIG_DFL);
	    SIGNAL (SIGQUIT, SIG_DFL);

	    close (pd[1]);
	    if (pd[0] != fileno (stdin)) {
		dup2 (pd[0], fileno (stdin));
		close (pd[0]);
	    }
	    execlp (name, r1bindex (name, '/'), NULL);
	    fprintf (stderr, "unable to exec ");
	    perror (name);
	    _exit (-1);

	default: 
	    close (pd[0]);
	    if (pd[1] != fileno (stdout)) {
		dup2 (pd[1], fileno (stdout));
		close (pd[1]);
	    }
    }
}


void
m_pclose (void)
{
    if (m_pid == NOTOK)
	return;

    if (sd != NOTOK) {
	fflush (stdout);
	if (dup2 (sd, fileno (stdout)) == NOTOK)
	    adios ("standard output", "unable to dup2()");

	clearerr (stdout);
	close (sd);
	sd = NOTOK;
    }
    else
	fclose (stdout);

    pidwait (m_pid, OK);
    m_pid = NOTOK;
}
