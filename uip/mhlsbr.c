/* mhlsbr.c -- main routines for nmh message lister
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/copyip.h"
#include "sbr/discard.h"
#include "sbr/trimcpy.h"
#include "sbr/vfgets.h"
#include "sbr/check_charset.h"
#include "sbr/getcpy.h"
#include "sbr/brkstring.h"
#include "sbr/ambigsw.h"
#include "sbr/pidstatus.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/arglist.h"
#include "sbr/error.h"
#include "h/signals.h"
#include "h/addrsbr.h"
#include "h/fmt_scan.h"
#include "h/tws.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/m_popen.h"
#include <setjmp.h>
#include <sys/types.h>
#include "sbr/terminal.h"

/*
 * MAJOR BUG:
 * for a component containing addresses, ADDRFMT, if COMPRESS is also
 * set, then addresses get split wrong (not at the spaces between commas).
 * To fix this correctly, putstr() should know about "atomic" strings that
 * must NOT be broken across lines.  That's too difficult for right now
 * (it turns out that there are a number of degenerate cases), so in
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

#define MHL_SWITCHES \
    X("bell", 0, BELLSW) \
    X("nobell", 0, NBELLSW) \
    X("clear", 0, CLRSW) \
    X("noclear", 0, NCLRSW) \
    X("folder +folder", 0, FOLDSW) \
    X("form formfile", 0, FORMSW) \
    X("moreproc program", 0, PROGSW) \
    X("nomoreproc", 0, NPROGSW) \
    X("length lines", 0, LENSW) \
    X("width columns", 0, WIDTHSW) \
    X("sleep seconds", 0, SLEEPSW) \
    X("dashstuffing", -12, BITSTUFFSW)    /* interface from forw */ \
    X("nodashstuffing", -14, NBITSTUFFSW) /* interface from forw */ \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("forward", -7, FORW1SW)             /* interface from forw */ \
    X("forwall", -7, FORW2SW)             /* interface from forw */ \
    X("digest list", -6, DGSTSW) \
    X("volume number", -6, VOLUMSW) \
    X("issue number", -5, ISSUESW) \
    X("nobody", -6, NBODYSW) \
    X("fmtproc program", 0, FMTPROCSW) \
    X("nofmtproc", 0, NFMTPROCSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MHL);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MHL, mhlswitches);
#undef X

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
#define RTRIM       0x004000	/* trim trailing whitespace          */
#define	SPLIT       0x010000	/* split headers (don't concatenate) */
#define	NONEWLINE   0x020000	/* don't write trailing newline      */
#define NOWRAP      0x040000	/* Don't wrap lines ever             */
#define FMTFILTER   0x080000	/* Filter through format filter      */
#define INVISIBLE   0x100000	/* count byte in display columns?    */
#define FORCE7BIT   0x200000	/* don't display 8-bit bytes	     */
#define	LBITS	"\020\01NOCOMPONENT\02UPPERCASE\03CENTER\04CLEARTEXT\05EXTRA\06HDROUTPUT\07CLEARSCR\010LEFTADJUST\011COMPRESS\012ADDRFMT\013BELL\014DATEFMT\015FORMAT\016INIT\017RTRIM\021SPLIT\022NONEWLINE\023NOWRAP\024FMTFILTER\025INVISIBLE\026FORCE7BIT"
#define	GFLAGS	(NOCOMPONENT | UPPERCASE | CENTER | LEFTADJUST | COMPRESS | SPLIT | NOWRAP)

/*
 * A format string to be used as a command-line argument to the body
 * format filter.
 */

struct arglist {
    struct format *a_fmt;
    char *a_nfs;
    struct arglist *a_next;
};

/*
 * Linked list of command line arguments for the body format filter.  This
 * USED to be in "struct mcomp", but the format API got cleaned up and even
 * though it reduced the code we had to do, it make things more complicated
 * for us.  Specifically:
 *
 * - The interface to the hash table has been cleaned up, which means the
 *   rooting around in the hash table is no longer necessary (yay!).  But
 *   this ALSO means that we have to make sure that we call our format
 *   compilation routines before we process the message, because the
 *   components need to be visible in the hash table so we can save them for
 *   later.  So we moved them out of "mcomp" and now compile them right before
 *   header processing starts.
 * - We also use format strings to handle other components in the mhl
 *   configuration (using "formatfield" and "decode"), but here life
 *   gets complicated: they aren't dealt with in the normal way.  Instead
 *   of referring to a component like {from}, each component is processed
 *   using the special {text} component.  But these format strings need to be
 *   compiled BEFORE we compile the format arguments; in the previous
 *   implementation they were compiled and scanned as the headers were
 *   read, and that would reset the hash table that we need to populate
 *   the components used by the body format filter.  So we are compiling
 *   the formatfield component strings ahead of time and then scanning them
 *   later.
 *
 * Okay, fine ... this was broken before.  But you know what?  Fixing this
 * the right way will make things easier down the road.
 *
 * One side-effect to this change: format strings are now compiled only once
 * for components specified with "formatfield", but they are compiled for
 * every message for format arguments.
 */

static struct arglist *arglist_head;
static struct arglist *arglist_tail;
static int filter_nargs = 0;

/*
 * Flags/options for each component
 */

struct mcomp {
    char *c_name;		/* component name          */
    char *c_text;		/* component text          */
    char *c_ovtxt;		/* text overflow indicator */
    char *c_nfs;		/* iff FORMAT              */
    struct format *c_fmt;	/*   ..                    */
    struct comp *c_c_text;	/* Ref to {text} in FORMAT */
    struct comp *c_c_error;	/* Ref to {error}          */
    int c_offset;		/* left margin indentation */
    int c_ovoff;		/* overflow indentation    */
    int c_width;		/* width of field          */
    int c_cwidth;		/* width of component      */
    int c_length;		/* length in lines         */
    unsigned long c_flags;
    struct mcomp *c_next;
};

static struct mcomp *msghd = NULL;
static struct mcomp *msgtl = NULL;
static struct mcomp *fmthd = NULL;
static struct mcomp *fmttl = NULL;

static struct mcomp global = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, -1, 80, -1, 40, BELL, NULL
};

static struct mcomp holder = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, NOCOMPONENT, NULL
};

struct pair {
    char *p_name;
    unsigned long p_flags;
};

static struct pair pairs[] = {
    { "Date",            DATEFMT },
    { "From",            ADDRFMT },
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
    { NULL,              0 }
};

struct triple {
    char *t_name;
    unsigned long t_on;
    unsigned long t_off;
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
    { "rtrim",         RTRIM,       0 },
    { "nortrim",       0,           RTRIM },
    { "addrfield",     ADDRFMT,     DATEFMT },
    { "bell",          BELL,        0 },
    { "nobell",        0,           BELL },
    { "datefield",     DATEFMT,     ADDRFMT },
    { "newline",       0,           NONEWLINE },
    { "nonewline",     NONEWLINE,   0 },
    { "wrap",          0,           NOWRAP },
    { "nowrap",        NOWRAP,      0 },
    { "format",        FMTFILTER,   0 },
    { "noformat",      0,           FMTFILTER },
    { NULL,            0,           0 }
};

static char *addrcomps[] = {
    "from",
    "sender",
    "reply-to",
    "to",
    "cc",
    "bcc",
    "resent-from",
    "resent-sender",
    "resent-reply-to",
    "resent-to",
    "resent-cc",
    "resent-bcc",
    NULL
};


static int bellflg   = 0;
static int clearflg  = 0;
static int dashstuff = 0;
static bool dobody = true;
static bool forwflg;
static bool forwall;

static int sleepsw = NOTOK;

static char *digest = NULL;
static int volume = 0;
static int issue = 0;

static int exitstat = 0;
static bool mhldebug;

static int filesize = 0;

#define	PITTY	(-1)
#define	NOTTY	0
#define	ISTTY	1
static int ontty = NOTTY;

static int row;
static unsigned int column;

static int lm;
static int llim;
static int ovoff;
static int term;
static unsigned int wid;

static char *ovtxt;

static char *onelp;

static char *parptr;

static int num_ignores = 0;
static char *ignores[MAXARGS];

static  jmp_buf env;

static char delim3[] =		/* from forw.c */
    "\n----------------------------------------------------------------------\n\n";
static char delim4[] = "\n------------------------------\n\n";

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
static int mcomp_flags (char *) PURE;
static char *mcomp_add (unsigned long, char *, char *);
static void mcomp_format (struct mcomp *, struct mcomp *);
static struct mcomp *add_queue (struct mcomp **, struct mcomp **, char *, char *, int);
static void free_queue (struct mcomp **, struct mcomp **);
static void putcomp (struct mcomp *, struct mcomp *, int);
static char *oneline (char *, unsigned long);
static void putstr (char *, unsigned long);
static void putch (char, unsigned long);
static bool linefeed_typed(void);
static void intrser (int);
static void pipeser (int);
static void quitser (int);
static void mhladios (char *, char *, ...) CHECK_PRINTF(2, 3) NORETURN;
static void mhldone (int) NORETURN;
static void filterbody (struct mcomp *, char *, int, int,
                        m_getfld_state_t);
static void compile_formatfield(struct mcomp *);
static void compile_filterargs (void);


int
mhl (int argc, char **argv)
{
    int length = 0;
    bool nomore = false;
    unsigned int i, vecp = 0;
    int width = 0;
    char *cp, *folder = NULL, *form = NULL;
    char buf[BUFSIZ], *files[MAXARGS];
    char **argp, **arguments;

    /* Need this if called from main() of show(1). */
    invo_name = r1bindex (argv[0], '/');

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    if ((cp = getenv ("MHLDEBUG")) && *cp)
	mhldebug = true;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, mhlswitches)) {
		case AMBIGSW: 
		    ambigsw (cp, mhlswitches);
		    mhldone (1);
		case UNKWNSW: 
		    mhladios (NULL, "-%s unknown\n", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [switches] [files ...]", invo_name);
		    print_help (buf, mhlswitches, 1);
		    mhldone (0);
		case VERSIONSW:
		    print_version(invo_name);
		    mhldone (0);

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
			mhladios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case FORMSW: 
		    if (!(form = *argp++) || *form == '-')
			mhladios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case SLEEPSW:
		    if (!(cp = *argp++) || *cp == '-')
			mhladios (NULL, "missing argument to %s", argp[-2]);
                    sleepsw = atoi (cp);/* ZERO ok! */
		    continue;

		case PROGSW:
		    if (!(moreproc = *argp++) || *moreproc == '-')
			mhladios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case NPROGSW:
		    nomore = true;
		    continue;

		case FMTPROCSW:
		    if (!(formatproc = *argp++) || *formatproc == '-')
			mhladios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case NFMTPROCSW:
		    formatproc = NULL;
		    continue;

		case LENSW: 
		    if (!(cp = *argp++) || *cp == '-')
			mhladios (NULL, "missing argument to %s", argp[-2]);
		    if ((length = atoi (cp)) < 1)
			mhladios (NULL, "bad argument %s %s", argp[-2], cp);
		    continue;
		case WIDTHSW: 
		    if (!(cp = *argp++) || *cp == '-')
			mhladios (NULL, "missing argument to %s", argp[-2]);
		    if ((width = atoi (cp)) < 1)
			mhladios (NULL, "bad argument %s %s", argp[-2], cp);
		    continue;

		case DGSTSW: 
		    if (!(digest = *argp++) || *digest == '-')
			mhladios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case ISSUESW:
		    if (!(cp = *argp++) || *cp == '-')
			mhladios (NULL, "missing argument to %s", argp[-2]);
		    if ((issue = atoi (cp)) < 1)
			mhladios (NULL, "bad argument %s %s", argp[-2], cp);
		    continue;
		case VOLUMSW:
		    if (!(cp = *argp++) || *cp == '-')
			mhladios (NULL, "missing argument to %s", argp[-2]);
		    if ((volume = atoi (cp)) < 1)
			mhladios (NULL, "bad argument %s %s", argp[-2], cp);
		    continue;

		case FORW2SW: 
		    forwall = true;
		    /* FALLTHRU */
		case FORW1SW: 
		    forwflg = true;
		    clearflg = -1;/* XXX */
		    continue;

		case BITSTUFFSW: 
		    dashstuff = 1;	/* ternary logic */
		    continue;
		case NBITSTUFFSW: 
		    dashstuff = -1;	/* ternary logic */
		    continue;

		case NBODYSW: 
		    dobody = false;
		    continue;
	    }
	}
	files[vecp++] = cp;
    }

    if (!folder)
	folder = getenv ("mhfolder");

    if (isatty (fileno (stdout))) {
	if (!nomore && moreproc && *moreproc != '\0') {
	    SIGNAL2 (SIGPIPE, pipeser);
	    m_popen(moreproc, false);
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
	    fputs(delim4, stdout);
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
	    fputs(buf, stdout);
	}
	else
	    printf ("\n------- End of Forwarded Message%s\n",
		    PLURALS(vecp));
    }

    fflush(stdout);
    if(ferror(stdout)){
	    mhladios("output", "error writing");
    }
    
    if (clearflg > 0 && ontty == NOTTY)
	nmh_clear_screen ();

    if (ontty == PITTY)
	m_pclose ();

    return exitstat;
}


static void
mhl_format (char *file, int length, int width)
{
    int i;
    char *bp, **ip;
    char *ap, name[NAMESZ];
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
        free_queue (&fmthd, &fmttl);
    }

    if ((fp = fopen (etcpath (file), "r")) == NULL)
	mhladios (file, "unable to open format file");

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
    filter_nargs = 0;

    while (vfgets (fp, &ap) == OK) {
	bp = ap;
	if (*bp == ';')
	    continue;

        trim_suffix_c(bp, '\n');

	if (*bp == ':') {
	    (void) add_queue (&fmthd, &fmttl, NULL, bp + 1, CLEARTEXT);
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
		    tmparray = brkstring (mh_xstrdup(++parptr), ",", NULL);

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
			mhladios (NULL, "format file syntax error: %s", bp);
		    if (*parptr)
			parptr++;
		}
		continue;

	    case ':': 
		c1 = add_queue (&fmthd, &fmttl, name, NULL, INIT);
		while (*parptr == ':' || *parptr == ',') {
		    parptr++;
		    if (evalvar (c1))
			mhladios (NULL, "format file syntax error: %s", bp);
		}
		if (!c1->c_nfs && global.c_nfs) {
		    if (c1->c_flags & DATEFMT) {
			if (global.c_flags & DATEFMT) {
			    c1->c_nfs = mh_xstrdup(global.c_nfs);
			    compile_formatfield(c1);
			}
		    } else if (c1->c_flags & ADDRFMT) {
                        if (global.c_flags & ADDRFMT) {
                            c1->c_nfs = mh_xstrdup(global.c_nfs);
                            compile_formatfield(c1);
                        }
                    }
		}
		continue;

	    default: 
		mhladios (NULL, "format file syntax error: %s", bp);
	}
    }
    fclose (fp);

    if (mhldebug) {
	for (c1 = fmthd; c1; c1 = c1->c_next) {
	    char buffer[BUFSIZ];

	    fprintf (stderr, "c1: name=\"%s\" text=\"%s\" ovtxt=\"%s\"\n",
		    c1->c_name, c1->c_text, c1->c_ovtxt);
	    fprintf(stderr, "\tnfs=%p fmt=%p\n",
                (void *)c1->c_nfs, (void *)c1->c_fmt);
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
	if (ptos (name, &cp))
	    return 1;
	c1->c_nfs = getcpy (new_fs (NULL, NULL, cp));
	compile_formatfield(c1);
	c1->c_flags |= FORMAT;
	return 0;
    }

    if (!strcasecmp (name, "decode")) {
	c1->c_nfs = getcpy (new_fs (NULL, NULL, "%(decode{text})"));
	compile_formatfield(c1);
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
	return dashstuff = -1;

    for (ap = triples; ap->t_name; ap++)
	if (!strcasecmp (ap->t_name, name)) {
	    c1->c_flags |= ap->t_on;
	    c1->c_flags &= ~ap->t_off;
	    return 0;
	}

   if (!strcasecmp (name, "formatarg")) {
	struct arglist *args;

	if (ptos (name, &cp))
	    return 1;

	if (! c1->c_name  ||  strcasecmp (c1->c_name, "body")) {
	    inform("format filters are currently only supported on "
	    	    "the \"body\" component");
	    return 1;
	}

	NEW0(args);

	if (arglist_tail)
	    arglist_tail->a_next = args;

	arglist_tail = args;

	if (! arglist_head)
	    arglist_head = args;

	args->a_nfs = getcpy (new_fs (NULL, NULL, cp));
	filter_nargs++;

	return 0;
    }

    return 1;
}


static int
ptoi (char *name, int *i)
{
    char *cp;

    if (*parptr++ != '=' || !*(cp = parse ())) {
	inform("missing argument to variable %s", name);
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
	inform("missing argument to variable %s", name);
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
    *s = mh_xstrdup(cp);
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
	if (!isalnum (c)
		&& c != '.'
		&& c != '-'
		&& c != '_'
		&& c !='['
		&& c != ']')
	    break;
        *cp++ = c;
    }
    *cp = '\0';

    return result;
}


/*
 * Process one file/message
 */

static void
process (char *folder, char *fname, int ofilen, int ofilec)
{
    /* static to prevent "might be clobbered" warning from gcc 4.9.2: */
    static char *cp;
    static FILE *fp;
    struct mcomp *c1;
    struct stat st;
    struct arglist *ap;
    /* volatile to prevent "might be clobbered" warning from gcc: */
    char *volatile fname2 = fname ? fname : "(stdin)";

    cp = NULL;
    fp = NULL;

    switch (setjmp (env)) {
	case OK: 
	    if (fname) {
		fp = fopen(fname, "r");
		if (fp == NULL) {
		    advise (fname, "unable to open");
		    exitstat++;
		    return;
		}
	    } else {
		fp = stdin;
	    }
	    if (fstat(fileno(fp), &st) == 0) {
	    	filesize = st.st_size;
	    } else {
	    	filesize = 0;
	    }
	    cp = folder ? concat (folder, ":", fname2, NULL) : mh_xstrdup(fname2);
	    if (ontty != PITTY)
		SIGNAL (SIGINT, intrser);
	    mhlfile (fp, cp, ofilen, ofilec);
            free (cp);

	    for (ap = arglist_head; ap; ap = ap->a_next) {
	    	fmt_free(ap->a_fmt, 0);
		ap->a_fmt = NULL;
	    }

	    if (arglist_head)
		fmt_free(NULL, 1);
	    /* FALLTHRU */

	default:
	    if (ontty != PITTY)
		SIGNAL (SIGINT, SIG_IGN);
	    if (fp != stdin && fp != NULL)
		fclose (fp);
            free(holder.c_text);
            holder.c_text = NULL;
	    free_queue (&msghd, &msgtl);
	    for (c1 = fmthd; c1; c1 = c1->c_next)
		c1->c_flags &= ~HDROUTPUT;
	    break;
    }

}


static void
mhlfile (FILE *fp, char *mname, int ofilen, int ofilec)
{
    int state, bucket;
    struct mcomp *c1, *c2, *c3;
    char **ip, name[NAMESZ], buf[NMH_BUFSIZ];
    m_getfld_state_t gstate;

    compile_filterargs();

    if (forwall) {
	if (digest)
	    fputs(ofilen == 1 ? delim3 : delim4, stdout);
	else {
	    fputs("\n-------", stdout);
	    if (ofilen == 1)
		printf (" Forwarded Message%s", PLURALS(ofilec));
	    else
		printf (" Message %d", ofilen);
	    puts("\n");
	}
    } else {
	switch (ontty) {
	    case PITTY: 
		if (ofilec > 1) {
		    if (ofilen > 1) {
			if ((global.c_flags & CLEARSCR))
			    nmh_clear_screen ();
			else
                            puts("\n\n");
		    }
		    printf (">>> %s\n\n", mname);
		}
		break;

	    case ISTTY: 
		if (ofilec > 1) {
		    if (SOprintf ("Press <return> to list \"%s\"...", mname)) {
			if (ofilen > 1)
			    puts("\n\n");
			printf ("Press <return> to list \"%s\"...", mname);
		    }
		    fflush (stdout);
		}
                if (ofilec == 1 || linefeed_typed()) {
		    if ((global.c_flags & CLEARSCR))
			nmh_clear_screen ();
		}
		else
		    putchar('\n');
		break;

	    default: 
		if (ofilec > 1) {
		    if (ofilen > 1) {
			puts("\n\n");
			if (clearflg > 0)
			    nmh_clear_screen ();
		    }
		    printf (">>> %s\n\n", mname);
		}
		break;
	}
    }

    gstate = m_getfld_state_init(fp);
    for (;;) {
	int bufsz = sizeof buf;
	switch (state = m_getfld2(&gstate, name, buf, &bufsz)) {
	    case FLD: 
	    case FLDPLUS: 
	        bucket = fmt_addcomptext(name, buf);
		for (ip = ignores; *ip; ip++)
		    if (!strcasecmp (name, *ip)) {
			while (state == FLDPLUS) {
			    bufsz = sizeof buf;
			    state = m_getfld2(&gstate, name, buf, &bufsz);
			    fmt_appendcomp(bucket, name, buf);
			}
			break;
		    }
		if (*ip)
		    continue;

		for (c2 = fmthd; c2; c2 = c2->c_next)
		    if (!strcasecmp (FENDNULL(c2->c_name), name))
			break;
		c1 = NULL;
		if (!((c3 = c2 ? c2 : &global)->c_flags & SPLIT))
		    for (c1 = msghd; c1; c1 = c1->c_next)
			if (!strcasecmp (FENDNULL(c1->c_name),
					 FENDNULL(c3->c_name))) {
			    c1->c_text =
				mcomp_add (c1->c_flags, buf, c1->c_text);
			    break;
			}
		if (c1 == NULL)
		    c1 = add_queue (&msghd, &msgtl, name, buf, 0);
		while (state == FLDPLUS) {
		    bufsz = sizeof buf;
		    state = m_getfld2(&gstate, name, buf, &bufsz);
		    c1->c_text = add (buf, c1->c_text);
		    fmt_appendcomp(bucket, name, buf);
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
		    if (!c1->c_name  ||
			!strcasecmp (c1->c_name, "messagename")) {
			holder.c_text = concat ("(Message ", mname, ")\n",
					    NULL);
			putcomp (c1, &holder, ONECOMP);
			free (holder.c_text);
			holder.c_text = NULL;
			continue;
		    }
		    if (!c1->c_name  ||  !strcasecmp (c1->c_name, "extras")) {
			for (c2 = msghd; c2; c2 = c2->c_next)
			    if (c2->c_flags & EXTRA)
				putcomp (c1, c2, TWOCOMP);
			continue;
		    }
		    if (dobody && (!c1->c_name  ||
				   !strcasecmp (c1->c_name, "body"))) {
		    	if (c1->c_flags & FMTFILTER && state == BODY &&
							formatproc != NULL) {
			    filterbody(c1, buf, sizeof(buf), state, gstate);
			} else {
			    holder.c_text = mh_xmalloc (sizeof(buf));
			    strncpy (holder.c_text, buf, sizeof(buf));
			    while (state == BODY) {
				putcomp (c1, &holder, BODYCOMP);
				bufsz = sizeof buf;
				state = m_getfld2(&gstate, name, holder.c_text,
					    &bufsz);
			    }
			    free (holder.c_text);
			    holder.c_text = NULL;
			}
			continue;
		    }
		    for (c2 = msghd; c2; c2 = c2->c_next)
			if (!strcasecmp (FENDNULL(c2->c_name),
					 FENDNULL(c1->c_name))) {
			    putcomp (c1, c2, ONECOMP);
			    if (!(c1->c_flags & SPLIT))
				break;
			}
		}
		m_getfld_state_destroy (&gstate);
		return;

	    case LENERR: 
	    case FMTERR: 
		inform("format error in message %s", mname);
		exitstat++;
		m_getfld_state_destroy (&gstate);
		return;

	    default: 
		mhladios (NULL, "getfld() returned %d", state);
	}
    }
}


static int
mcomp_flags (char *name)
{
    struct pair *ap;

    for (ap = pairs; ap->p_name; ap++)
	if (!strcasecmp (ap->p_name, name))
	    return ap->p_flags;

    return 0;
}


static char *
mcomp_add (unsigned long flags, char *s1, char *s2)
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
    char error[BUFSIZ];
    struct pqpair *p, *q;
    struct pqpair pq;
    struct mailname *mp;

    ap = c2->c_text;
    c2->c_text = NULL;
    dat[0] = 0;
    dat[1] = 0;
    dat[2] = filesize;
    dat[3] = BUFSIZ - 1;
    dat[4] = 0;

    if (!(c1->c_flags & ADDRFMT)) {
	charstring_t scanl = charstring_create (BUFSIZ);

	if (c1->c_c_text)
	    c1->c_c_text->c_text = ap;
	if ((cp = strrchr(ap, '\n')))	/* drop ending newline */
	    if (!cp[1])
		*cp = 0;

	fmt_scan (c1->c_fmt, scanl, BUFSIZ - 1, dat, NULL);
	/* Don't need to append a newline, dctime() already did */
	c2->c_text = charstring_buffer_copy (scanl);
	charstring_free (scanl);

	/* ap is now owned by the component struct, so do NOT free it here */
	return;
    }

    (q = &pq)->pq_next = NULL;
    while ((cp = getname (ap))) {
	NEW0(p);
        if ((mp = getm (cp, NULL, 0, error, sizeof(error))) == NULL) {
            p->pq_text = mh_xstrdup(cp);
            p->pq_error = mh_xstrdup(error);
        } else {
            p->pq_text = getcpy (mp->m_text);
            mnfree (mp);
        }
        q = (q->pq_next = p);
    }

    for (p = pq.pq_next; p; p = q) {
	charstring_t scanl = charstring_create (BUFSIZ);
	char *buffer;

	if (c1->c_c_text) {
	    c1->c_c_text->c_text = p->pq_text;
	    p->pq_text = NULL;
	}
	if (c1->c_c_error) {
	    c1->c_c_error->c_text = p->pq_error;
	    p->pq_error = NULL;
	}

	fmt_scan (c1->c_fmt, scanl, BUFSIZ - 1, dat, NULL);
	buffer = charstring_buffer_copy (scanl);
	if (*buffer) {
	    if (c2->c_text)
		c2->c_text = add (",\n", c2->c_text);
	    if (*(cp = buffer + strlen (buffer) - 1) == '\n')
		*cp = 0;
	    c2->c_text = add (buffer, c2->c_text);
	}
	charstring_free (scanl);

        free(p->pq_text);
        free(p->pq_error);
	q = p->pq_next;
	free(p);
    }

    c2->c_text = add ("\n", c2->c_text);
    free (ap);
}


static struct mcomp *
add_queue (struct mcomp **head, struct mcomp **tail, char *name, char *text, int flags)
{
    struct mcomp *c1;

    NEW0(c1);
    c1->c_flags = flags & ~INIT;
    if ((c1->c_name = name ? mh_xstrdup(name) : NULL))
        c1->c_flags |= mcomp_flags (c1->c_name);
    c1->c_text = text ? mh_xstrdup(text) : NULL;
    if (flags & INIT) {
        if (global.c_ovtxt)
            c1->c_ovtxt = mh_xstrdup(global.c_ovtxt);
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
        free(c1->c_name);
        free(c1->c_text);
        free(c1->c_ovtxt);
        free(c1->c_nfs);
	if (c1->c_fmt)
	    fmt_free (c1->c_fmt, 0);
	free(c1);
    }

    *head = *tail = NULL;
}


static void
putcomp (struct mcomp *c1, struct mcomp *c2, int flag)
{
    char *text; /* c1's text, or the name as a fallback. */
    char *trimmed_prefix;
    int count;
    bool cchdr;
    char *cp;
    const int utf8 = strcasecmp(get_charset(), "UTF-8") == 0;

    if (! utf8  &&  flag != BODYCOMP) {
        /* Don't print 8-bit bytes in header field values if not in a
           UTF-8 locale, as required by RFC 6532. */
	c1->c_flags |= FORCE7BIT;
    }

    text = c1->c_text ? c1->c_text : c1->c_name;
    /* Create a copy with trailing whitespace trimmed, for use with
     * blank lines. */
    trimmed_prefix = rtrim(mh_xstrdup(FENDNULL(text)));

    cchdr = false;
    lm = 0;
    llim = c1->c_length ? c1->c_length : -1;
    wid = c1->c_width ? c1->c_width : global.c_width;
    ovoff = (c1->c_ovoff >= 0 ? c1->c_ovoff : global.c_ovoff)
	+ c1->c_offset;
    if ((ovtxt = c1->c_ovtxt ? c1->c_ovtxt : global.c_ovtxt) == NULL)
	ovtxt = "";
    if (wid < ovoff + strlen (ovtxt) + 5)
	mhladios(NULL, "component: %s width(%d) too small for overflow(%zu)",
		c1->c_name, wid, ovoff + strlen (ovtxt) + 5);
    onelp = NULL;

    if (c1->c_flags & CLEARTEXT) {
	putstr (c1->c_flags & RTRIM ? rtrim (c1->c_text) : c1->c_text,
		c1->c_flags);
	putstr ("\n", c1->c_flags);
	return;
    }

    if (c1->c_nfs && (c1->c_flags & (ADDRFMT | DATEFMT | FORMAT)))
	mcomp_format (c1, c2);

    if (c1->c_flags & CENTER) {
	count = (c1->c_width ? c1->c_width : global.c_width)
	    - c1->c_offset - strlen (c2->c_text);
	if (!(c1->c_flags & HDROUTPUT) && !(c1->c_flags & NOCOMPONENT))
	    count -= strlen(text) + 2;
	lm = c1->c_offset + (count / 2);
    } else {
	if (c1->c_offset)
	    lm = c1->c_offset;
    }

    if (!(c1->c_flags & HDROUTPUT) && !(c1->c_flags & NOCOMPONENT)) {
        if (c1->c_flags & UPPERCASE)		/* uppercase component also */
            to_upper(text);
	putstr(text, c1->c_flags);
	if (flag != BODYCOMP) {
	    putstr (": ", c1->c_flags);
	    if (!(c1->c_flags & SPLIT))
		c1->c_flags |= HDROUTPUT;

	cchdr = true;
	if ((count = c1->c_cwidth - strlen(text) - 2) > 0)
	    while (count--)
		putstr (" ", c1->c_flags);
	}
	else
	    c1->c_flags |= HDROUTPUT;		/* for BODYCOMP */
    }

    if (flag == TWOCOMP
	    && !(c2->c_flags & HDROUTPUT)
	    && !(c2->c_flags & NOCOMPONENT)) {
        if (c1->c_flags & UPPERCASE)
            to_upper(c2->c_name);
	putstr (c2->c_name, c1->c_flags);
	putstr (": ", c1->c_flags);
	if (!(c1->c_flags & SPLIT))
	    c2->c_flags |= HDROUTPUT;

	cchdr = true;
	if ((count = c1->c_cwidth - strlen (c2->c_name) - 2) > 0)
	    while (count--)
		putstr (" ", c1->c_flags);
    }
    if (c1->c_flags & UPPERCASE)
        to_upper(c2->c_text);

    count = 0;
    if (cchdr) {
	if (flag == TWOCOMP)
	    count = (c1->c_cwidth >= 0) ? c1->c_cwidth
			: (int) strlen (c2->c_name) + 2;
	else
	    count = (c1->c_cwidth >= 0) ? (size_t) c1->c_cwidth
			: strlen(text) + 2;
    }
    count += c1->c_offset;

    if ((cp = oneline (c2->c_text, c1->c_flags)))
	/* Output line, trimming trailing whitespace if requested. */
	putstr (c1->c_flags & RTRIM ? rtrim (cp) : cp, c1->c_flags);
    if (term == '\n')
	putstr ("\n", c1->c_flags);
    while ((cp = oneline (c2->c_text, c1->c_flags))) {
	lm = count;
	if (flag == BODYCOMP
		&& !(c1->c_flags & NOCOMPONENT)) {
	    /* Output component, trimming trailing whitespace if there
	       is no text on the line. */
	    if (*cp) {
		putstr(text, c1->c_flags);
	    } else {
		putstr (trimmed_prefix, c1->c_flags);
	    }
	}
	if (*cp) {
	    /* Output line, trimming trailing whitespace if requested. */
	    putstr (c1->c_flags & RTRIM ? rtrim (cp) : cp, c1->c_flags);
        }
	if (term == '\n')
	    putstr ("\n", c1->c_flags);
    }
    if (flag == BODYCOMP && term == '\n')
	c1->c_flags &= ~HDROUTPUT;		/* Buffer ended on a newline */

    free (trimmed_prefix);
}


static char *
oneline (char *stuff, unsigned long flags)
{
    bool spc;
    char *cp, *ret;

    if (onelp == NULL)
	onelp = stuff;
    if (*onelp == 0)
	return onelp = NULL;

    ret = onelp;
    term = 0;
    if (flags & COMPRESS) {
	for (spc = true, cp = ret; *onelp; onelp++)
	    if (isspace ((unsigned char) *onelp)) {
		if (*onelp == '\n' && (!onelp[1] || (flags & ADDRFMT))) {
		    term = '\n';
		    *onelp++ = 0;
		    break;
		}
                if (!spc) {
                    *cp++ = ' ';
                    spc = true;
                }
	    }
	    else {
		*cp++ = *onelp;
		spc = false;
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
putstr (char *string, unsigned long flags)
{
    /* To not count, for the purpose of counting columns, all of
       the bytes of a multibyte character. */
    int char_len;

    if (!column && lm > 0) {
	while (lm > 0)
	    if (lm >= 8) {
		putch ('\t', flags);
		lm -= 8;
	    }
	    else {
		putch (' ', flags);
		lm--;
	    }
    }
    lm = 0;

#ifdef MULTIBYTE_SUPPORT
    if (mbtowc (NULL, NULL, 0)) {} /* reset shift state */
    char_len = 0;
#else
    NMH_UNUSED (char_len);
#endif

    while (*string) {
        flags &= ~INVISIBLE;
#ifdef MULTIBYTE_SUPPORT
        /* mbtowc should never return 0, because *string is non-NULL. */
        if (char_len <= 0) {
            /* Find number of bytes in next character. */
            if ((char_len =
                 mbtowc (NULL, string, (size_t) MB_CUR_MAX)) == -1) {
                char_len = 1;
            }
        } else {
            /* Multibyte character, after the first byte. */
            flags |= INVISIBLE;
        }

        --char_len;
#endif
        putch (*string++, flags);
    }
}


static void
putch (char ch, unsigned long flags)
{
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
            if (linefeed_typed()) {
		if (global.c_flags & CLEARSCR)
		    nmh_clear_screen ();
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
            /*
             * Increment the character count, unless
             * 1) In UTF-8 locale, this is other than the last byte of
                  a multibyte character, or
             * 2) In C locale, will print a non-printable character.
             */
            if ((flags & FORCE7BIT) == 0) {
                /* UTF-8 locale */
                if ((flags & INVISIBLE) == 0) {
                    /* If multibyte character, its first byte only. */
                    ++column;
                }
            } else {
                /* If not an ASCII character, the replace character will be
                   displayed.  Count it. */
                if (! isascii((unsigned char) ch) || isprint((unsigned char) ch)) {
                    ++column;
                }
            }
	    break;
    }

    if (column >= wid && (flags & NOWRAP) == 0) {
	putch ('\n', flags);
	if (ovoff > 0)
	    lm = ovoff;
	putstr (FENDNULL(ovtxt), flags);
	putch (ch, flags);
	return;
    }

    if (flags & FORCE7BIT  &&  ! isascii((unsigned char) ch)) {
        putchar ('?');
    } else {
        putchar (ch);
    }
}

/* linefeed_typed() makes a single read(2) from stdin and returns true
 * if a linefeed character is amongst the characters read.
 * A read error is treated as if linefeed wasn't typed.
 *
 * Typing on a TTY can cause read() to return data without typing Enter
 * by using the TTY's EOF character instead, normally ASCII EOT, Ctrl-D.
 * The linefeed can also be escaped with the TTY's LNEXT character,
 * normally ASCII SYN, Ctrl-V, by typing Ctrl-V Ctrl-J.
 * It's not possible to distinguish between the user typing a buffer's
 * worth of characters and then EOT, or more than the buffer can hold.
 * Either way, the result depends on ASCII LF, either from typing Enter
 * or an escaped Ctrl-J, being amongst the read characters.
 */
static bool
linefeed_typed(void)
{
    char buf[128];
    ssize_t n;

    n = read(0, buf, sizeof buf);
    if (n == -1) {
        advise("stdin", "read");
        return false; /* Treat as EOF. */
    }

    return memchr(buf, '\n', n);
}


static void
intrser (int i)
{
    NMH_UNUSED (i);

    discard (stdout);
    putchar ('\n');
    longjmp (env, DONE);
}


static void
pipeser (int i)
{
    NMH_UNUSED (i);

    mhldone (NOTOK);
}


static void
quitser (int i)
{
    NMH_UNUSED (i);

    putchar ('\n');
    fflush (stdout);
    mhldone (NOTOK);
}


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
    done (exitstat);
}


/*
 * Compile a format string used by the formatfield option and save it
 * for later.
 *
 * We will want the {text} (and possibly {error}) components for later,
 * so look for them and save them if we find them.
 */

static void
compile_formatfield(struct mcomp *c1)
{
    fmt_compile(c1->c_nfs, &c1->c_fmt, 1);

    /*
     * As a note to myself and any other poor bastard who is looking through
     * this code in the future ....
     *
     * When the format hash table is reset later on (as it almost certainly
     * will be), there will still be references to these components in the
     * compiled format instructions.  Thus these component references will
     * be free'd when the format instructions are free'd (by fmt_free()).
     *
     * So, in other words ... don't go free'ing them yourself!
     */

    c1->c_c_text = fmt_findcomp("text");
    c1->c_c_error = fmt_findcomp("error");
}

/*
 * Compile all of the arguments for our format list.
 *
 * Iterate through the linked list of format strings and compile them.
 * Note that we reset the format hash table before we start, but we do NOT
 * reset it between calls to fmt_compile().
 *
 */

static void
compile_filterargs (void)
{
    struct arglist *arg = arglist_head;
    struct comp *cptr;
    char **ap;

    fmt_free(NULL, 1);

    while (arg) {
    	fmt_compile(arg->a_nfs, &arg->a_fmt, 0);
	arg = arg->a_next;
    }

    /*
     * Search through and mark any components that are address components
     */

    for (ap = addrcomps; *ap; ap++) {
	cptr = fmt_findcomp (*ap);
	if (cptr)
	    cptr->c_type |= CT_ADDR;
    }
}

/*
 * Filter the body of a message through a specified format program
 */

static void
filterbody (struct mcomp *c1, char *buf, int bufsz, int state,
    m_getfld_state_t gstate)
{
    struct mcomp holder;
    char name[NAMESZ];
    int fdinput[2], fdoutput[2], waitstat;
    ssize_t cc;
    pid_t writerpid, filterpid;

    /*
     * Create pipes so we can communicate with our filter process.
     */

    if (pipe(fdinput) < 0) {
	die("Unable to create input pipe");
    }

    if (pipe(fdoutput) < 0) {
    	die("Unable to create output pipe");
    }

    /*
     * Here's what we're doing to do.
     *
     * - Fork ourselves and start writing data to the write side of the
     *   input pipe (fdinput[1]).
     *
     * - Fork and exec our filter program.  We set the standard input of
     *   our filter program to be the read side of our input pipe (fdinput[0]).
     *   Standard output is set to the write side of our output pipe
     *   (fdoutput[1]).
     *
     * - We read from the read side of the output pipe (fdoutput[0]).
     *
     * We're forking because that's the simplest way to prevent any deadlocks.
     * (without doing something like switching to non-blocking I/O and using
     * select or poll, and I'm not interested in doing that).
     */

    switch (writerpid = fork()) {
    case 0:
    	/*
	 * Our child process - just write to the filter input (fdinput[1]).
	 * Close all other descriptors that we don't need.
	 */

	close(fdinput[0]);
	close(fdoutput[0]);
	close(fdoutput[1]);

	/*
	 * Call m_getfld2() until we're no longer in the BODY state
	 */

	while (state == BODY) {
	    int bufsz2 = bufsz;
	    if (write(fdinput[1], buf, strlen(buf)) < 0) {
		advise ("pipe output", "write");
	    }
	    state = m_getfld2(&gstate, name, buf, &bufsz2);
	}

	/*
	 * We should be done; time to exit.
	 */

	close(fdinput[1]);
	/*
	 * Make sure we call _exit(), otherwise we may flush out the stdio
	 * buffers that we have duplicated from the parent.
	 */
	_exit(0);
    case -1:
    	die("Unable to fork for filter writer process");
	break;
    }

    /*
     * Fork and exec() our filter program, after redirecting standard in
     * and standard out appropriately.
     */

    switch (filterpid = fork()) {
        char **args, *program;
	struct arglist *a;
	int i, dat[5], s, argp;

    case 0:
    	/*
	 * Configure an argument array for us
	 */

	args = argsplit(formatproc, &program, &argp);
	args[argp + filter_nargs] = NULL;
	dat[0] = 0;
	dat[1] = 0;
	dat[2] = 0;
	dat[3] = BUFSIZ;
	dat[4] = 0;

	/*
	 * Pull out each argument and scan them.
	 */

	for (a = arglist_head, i = argp; a != NULL; a = a->a_next, i++) {
	    charstring_t scanl = charstring_create (BUFSIZ);

	    fmt_scan(a->a_fmt, scanl, BUFSIZ, dat, NULL);
	    args[i] = charstring_buffer_copy (scanl);
	    charstring_free (scanl);
	    /*
	     * fmt_scan likes to put a trailing newline at the end of the
	     * format string.  If we have one, get rid of it.
	     */
	    s = strlen(args[i]);
	    if (args[i][s - 1] == '\n')
		args[i][s - 1] = '\0';

	    if (mhldebug)
		fprintf(stderr, "filterarg: fmt=\"%s\", output=\"%s\"\n",
			a->a_nfs, args[i]);
	}

	if (dup2(fdinput[0], STDIN_FILENO) < 0) {
	    adios("formatproc", "Unable to dup2() standard input");
	}
	if (dup2(fdoutput[1], STDOUT_FILENO) < 0) {
	    adios("formatproc", "Unable to dup2() standard output");
	}

	/*
	 * Close everything (especially the old input and output
	 * descriptors, since they've been dup'd to stdin and stdout),
	 * and exec the formatproc.
	 */

	close(fdinput[0]);
	close(fdinput[1]);
	close(fdoutput[0]);
	close(fdoutput[1]);

	execvp(formatproc, args);

	adios(formatproc, "Unable to execute filter");

	break;

    case -1:
    	die("Unable to fork format program");
    }

    /*
     * Close everything except our reader (fdoutput[0]);
     */

    close(fdinput[0]);
    close(fdinput[1]);
    close(fdoutput[1]);

    /*
     * As we read in this data, send it to putcomp
     */

    holder.c_text = buf;

    while ((cc = read(fdoutput[0], buf, bufsz - 1)) > 0) {
    	buf[cc] = '\0';
    	putcomp(c1, &holder, BODYCOMP);
    }

    if (cc < 0) {
    	die("reading from formatproc");
    }

    /*
     * See if we got any errors along the way.  I'm a little leery of calling
     * waitpid() without WNOHANG, but it seems to be the most correct solution.
     */

    if (waitpid(filterpid, &waitstat, 0) < 0) {
    	if (errno != ECHILD) {
	    adios("filterproc", "Unable to determine status");
	}
    } else {
    	if (! (WIFEXITED(waitstat) && WEXITSTATUS(waitstat) == 0)) {
	    pidstatus(waitstat, stderr, "filterproc");
	}
    }

    if (waitpid(writerpid, &waitstat, 0) < 0) {
    	if (errno != ECHILD) {
	    adios("writer process", "Unable to determine status");
	    done(1);
	}
    } else {
    	if (! (WIFEXITED(waitstat) && WEXITSTATUS(waitstat) == 0)) {
	    pidstatus(waitstat, stderr, "writer process");
	    done(1);
	}
    }

    close(fdoutput[0]);
}
