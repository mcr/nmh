
/*
 * spost.c -- feed messages to sendmail
 *
 * This is a simpler, faster, replacement for "post" for use
 * when "sendmail" is the transport system.
 *
 * $Id$
 */

#include <h/mh.h>
#include <signal.h>
#include <h/addrsbr.h>
#include <h/aliasbr.h>
#include <h/dropsbr.h>
#include <zotnet/tws/tws.h>
#include <zotnet/mts/mts.h>

#define	uptolow(c)	((isalpha(c) && isupper (c)) ? tolower (c) : c)

#define MAX_SM_FIELD	1476	/* < largest hdr field sendmail will accept */
#define FCCS		10	/* max number of fccs allowed */

struct swit switches[] = {
#define	FILTSW               0
    { "filter filterfile", 0 },
#define	NFILTSW              1
    { "nofilter", 0 },
#define	FRMTSW               2
    { "format", 0 },
#define	NFRMTSW              3
    { "noformat", 0 },
#define	REMVSW               4
    { "remove", 0 },
#define	NREMVSW              5
    { "noremove", 0 },
#define	VERBSW               6
    { "verbose", 0 },
#define	NVERBSW              7
    { "noverbose", 0 },
#define	WATCSW               8
    { "watch", 0 },
#define	NWATCSW              9
    { "nowatch", 0 },
#define BACKSW              10
    { "backup", 0 },
#define NBACKSW             11
    { "nobackup", 0 },
#define ALIASW              12
    { "alias aliasfile", 0 },
#define NALIASW             13
    { "noalias", 0 },
#define WIDTHSW             14
    { "width columns", 0 },
#define VERSIONSW           15
    { "version", 0 },
#define	HELPSW              16
    { "help", 4 },
#define	DEBUGSW             17
    { "debug", -5 },
#define	DISTSW              18
    { "dist", -4 },		/* interface from dist */
#define CHKSW               19
    { "check", -5 },		/* interface from whom */
#define NCHKSW              20
    { "nocheck", -7 },		/* interface from whom */
#define WHOMSW              21
    { "whom", -4 },		/* interface from whom */
#define PUSHSW              22	/* fork to sendmail then exit */
    { "push", -4 },
#define NPUSHSW             23	/* exec sendmail */
    { "nopush", -6 },
#define LIBSW               24
    { "library directory", -7 },
#define	ANNOSW              25
    { "idanno number", -6 },
    { NULL, 0 }
};


/* flags for headers->flags */
#define	HNOP	0x0000		/* just used to keep .set around */
#define	HBAD	0x0001		/* bad header - don't let it through */
#define	HADR	0x0002		/* header has an address field */
#define	HSUB	0x0004		/* Subject: header */
#define	HTRY	0x0008		/* try to send to addrs on header */
#define	HBCC	0x0010		/* don't output this header */
#define	HMNG	0x0020		/* mung this header */
#define	HNGR	0x0040		/* no groups allowed in this header */
#define	HFCC	0x0080		/* FCC: type header */
#define	HNIL	0x0100		/* okay for this header not to have addrs */
#define	HIGN	0x0200		/* ignore this header */

/* flags for headers->set */
#define	MFRM	0x0001		/* we've seen a From: */
#define	MDAT	0x0002		/* we've seen a Date: */
#define	MRFM	0x0004		/* we've seen a Resent-From: */
#define	MVIS	0x0008		/* we've seen sighted addrs */
#define	MINV	0x0010		/* we've seen blind addrs */
#define	MRDT	0x0020		/* we've seen a Resent-Date: */

struct headers {
    char *value;
    unsigned int flags;
    unsigned int set;
};


static struct headers NHeaders[] = {
    { "Return-Path", HBAD,                0 },
    { "Received",    HBAD,                0 },
    { "Reply-To",    HADR|HNGR,           0 },
    { "From",        HADR|HNGR,           MFRM },
    { "Sender",      HADR|HBAD,           0 },
    { "Date",        HNOP,                MDAT },
    { "Subject",     HSUB,                0 },
    { "To",          HADR|HTRY,           MVIS },
    { "cc",          HADR|HTRY,           MVIS },
    { "Bcc",         HADR|HTRY|HBCC|HNIL, MINV },
    { "Message-Id",  HBAD,                0 },
    { "Fcc",         HFCC,                0 },
    { NULL,          0,                   0 }
};

static struct headers RHeaders[] = {
    { "Resent-Reply-To",   HADR|HNGR,      0 },
    { "Resent-From",       HADR|HNGR,      MRFM },
    { "Resent-Sender",     HADR|HBAD,      0 },
    { "Resent-Date",       HNOP,           MRDT },
    { "Resent-Subject",    HSUB,           0 },
    { "Resent-To",         HADR|HTRY,      MVIS },
    { "Resent-cc",         HADR|HTRY,      MVIS },
    { "Resent-Bcc",        HADR|HTRY|HBCC, MINV },
    { "Resent-Message-Id", HBAD,           0 },
    { "Resent-Fcc",        HFCC,           0 },
    { "Reply-To",          HADR,           0 },
    { "Fcc",               HIGN,           0 },
    { NULL,                0,              0 }
};


static short fccind = 0;	/* index into fccfold[] */

static int badmsg = 0;		/* message has bad semantics            */
static int verbose = 0;		/* spell it out                         */
static int debug = 0;		/* debugging post                       */
static int rmflg = 1;		/* remove temporary file when done      */
static int watch = 0;		/* watch the delivery process           */
static int backflg = 0;		/* rename input file as *.bak when done */
static int whomflg = 0;		/* if just checking addresses           */
static int pushflg = 0;		/* if going to fork to sendmail         */
static int aliasflg = -1;	/* if going to process aliases          */
static int outputlinelen=72;

static unsigned msgflags = 0;	/* what we've seen */

static enum {
    normal, resent
} msgstate = normal;

static char tmpfil[] = "/tmp/pstXXXXXX";

static char from[BUFSIZ];	/* my network address */
static char signature[BUFSIZ];	/* my signature */
static char *filter = NULL;	/* the filter for BCC'ing */
static char *subject = NULL;	/* the subject field for BCC'ing */
static char *fccfold[FCCS];	/* foldernames for FCC'ing */

static struct headers *hdrtab;	/* table for the message we're doing */
static FILE *out;		/* output (temp) file */

extern char *sendmail;

/*
 * external prototypes
 */
extern char *getfullname (void);
extern char *getusername (void);

/*
 * static prototypes
 */
static void putfmt (char *, char *, FILE *);
static void start_headers (void);
static void finish_headers (FILE *);
static int get_header (char *, struct headers *);
static void putadr (char *, struct mailname *);
static int putone (char *, int, int);
static void insert_fcc (struct headers *, char *);
static void file (char *);
static void fcc (char *, char *);

#if 0
static void die (char *, char *, ...);
static void make_bcc_file (void);
#endif


int
main (int argc, char **argv)
{
    int state, i, pid, compnum;
    char *cp, *msg = NULL, **argp, **arguments;
    char *sargv[16], buf[BUFSIZ], name[NAMESZ];
    FILE *in;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* foil search of user profile/context */
    if (context_foil (NULL) == -1)
	done (1);

    mts_init (invo_name);
    arguments = getarguments (invo_name, argc, argv, 0);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [switches] file", invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case DEBUGSW: 
		    debug++;
		    continue;

		case DISTSW:
		    msgstate = resent;
		    continue;

		case WHOMSW:
		    whomflg++;
		    continue;

		case FILTSW:
		    if (!(filter = *argp++) || *filter == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case NFILTSW:
		    filter = NULL;
		    continue;
		
		case REMVSW: 
		    rmflg++;
		    continue;
		case NREMVSW: 
		    rmflg = 0;
		    continue;

		case BACKSW: 
		    backflg++;
		    continue;
		case NBACKSW: 
		    backflg = 0;
		    continue;

		case VERBSW: 
		    verbose++;
		    continue;
		case NVERBSW: 
		    verbose = 0;
		    continue;

		case WATCSW: 
		    watch++;
		    continue;
		case NWATCSW: 
		    watch = 0;
		    continue;
		
		case PUSHSW:
		    pushflg++;
		    continue;
		case NPUSHSW:
		    pushflg = 0;
		    continue;

		case ALIASW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if (aliasflg < 0)
			alias (AliasFile);/* load default aka's */
		    aliasflg = 1;
		    if ((state = alias(cp)) != AK_OK)
			adios (NULL, "aliasing error in file %s - %s",
			       cp, akerror(state) );
		    continue;
		case NALIASW:
		    aliasflg = 0;
		    continue;

		case WIDTHSW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    outputlinelen = atoi (cp);
		    if (outputlinelen <= 10)
			outputlinelen = 72;
		    continue;

		case LIBSW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    /* create a minimal context */
		    if (context_foil (cp) == -1)
			done(1);
		    continue;

		case ANNOSW:
		    /* -idanno switch ignored */
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
	    }
	}
	if (msg)
	    adios (NULL, "only one message at a time!");
	else
	    msg = cp;
    }

    if (aliasflg < 0)
	alias (AliasFile);	/* load default aka's */

    if (!msg)
	adios (NULL, "usage: %s [switches] file", invo_name);

    if ((in = fopen (msg, "r")) == NULL)
	adios (msg, "unable to open");

    start_headers ();
    if (debug) {
	verbose++;
	out = stdout;
    }
    else {
#ifdef HAVE_MKSTEMP
	    if ((out = fdopen( mkstemp (tmpfil), "w" )) == NULL )
		adios (tmpfil, "unable to create");
#else
	    mktemp (tmpfil);
	    if ((out = fopen (tmpfil, "w")) == NULL)
		adios (tmpfil, "unable to create");
	    chmod (tmpfil, 0600);
#endif
	}

    hdrtab = (msgstate == normal) ? NHeaders : RHeaders;

    for (compnum = 1, state = FLD;;) {
	switch (state = m_getfld (state, name, buf, sizeof(buf), in)) {
	    case FLD: 
		compnum++;
		putfmt (name, buf, out);
		continue;

	    case FLDPLUS: 
		compnum++;
		cp = add (buf, cp);
		while (state == FLDPLUS) {
		    state = m_getfld (state, name, buf, sizeof(buf), in);
		    cp = add (buf, cp);
		}
		putfmt (name, cp, out);
		free (cp);
		continue;

	    case BODY: 
		finish_headers (out);
		fprintf (out, "\n%s", buf);
		if(whomflg == 0)
		    while (state == BODY) {
			state = m_getfld (state, name, buf, sizeof(buf), in);
			fputs (buf, out);
		    }
		break;

	    case FILEEOF: 
		finish_headers (out);
		break;

	    case LENERR: 
	    case FMTERR: 
		adios (NULL, "message format error in component #%d",
			compnum);

	    default: 
		adios (NULL, "getfld() returned %d", state);
	}
	break;
    }

    fclose (in);
    if (backflg && !whomflg) {
	strncpy (buf, m_backup (msg), sizeof(buf));
	if (rename (msg, buf) == NOTOK)
	    advise (buf, "unable to rename %s to", msg);
    }

    if (debug) {
	done (0);
    }
    else
	fclose (out);

    file (tmpfil);

    /*
     * re-open the temp file, unlink it and exec sendmail, giving it
     * the msg temp file as std in.
     */
    if ( freopen( tmpfil, "r", stdin) == NULL)
	adios (tmpfil, "can't reopen for sendmail");
    if (rmflg)
	unlink (tmpfil);

    argp = sargv;
    *argp++ = "send-mail";
    *argp++ = "-m";	/* send to me too */
    *argp++ = "-t";	/* read msg for recipients */
    *argp++ = "-i";	/* don't stop on "." */
    if (whomflg)
	*argp++ = "-bv";
    if (watch || verbose)
	*argp++ = "-v";
    *argp = NULL;

    if (pushflg && !(watch || verbose)) {
	/* fork to a child to run sendmail */
	for (i=0; (pid = vfork()) == NOTOK && i < 5; i++)
	    sleep(5);
	switch (pid) {
	    case NOTOK:
		fprintf (verbose ? stdout : stderr, "%s: can't fork to %s\n",
			 invo_name, sendmail);
		exit(-1);
	    case OK:
		/* we're the child .. */
		break;
	    default:
		exit(0);
	}
    }
    execv ( sendmail, sargv);
    adios ( sendmail, "can't exec");
    return 0;  /* dead code to satisfy the compiler */
}

/* DRAFT GENERATION */

static void
putfmt (char *name, char *str, FILE *out)
{
    int i;
    char *cp, *pp;
    struct headers *hdr;

    while (*str == ' ' || *str == '\t')
	str++;

    if ((i = get_header (name, hdrtab)) == NOTOK) {
	fprintf (out, "%s: %s", name, str);
	return;
    }

    hdr = &hdrtab[i];
    if (hdr->flags & HIGN)
	return;
    if (hdr->flags & HBAD) {
	advise (NULL, "illegal header line -- %s:", name);
	badmsg++;
	return;
    }
    msgflags |= hdr->set;

    if (hdr->flags & HSUB)
	subject = subject ? add (str, add ("\t", subject)) : getcpy (str);

    if (hdr->flags & HFCC) {
	if ((cp = strrchr(str, '\n')))
	    *cp = 0;
	for (cp = pp = str; (cp = strchr(pp, ',')); pp = cp) {
	    *cp++ = 0;
	    insert_fcc (hdr, pp);
	}
	insert_fcc (hdr, pp);
	return;
    }

#ifdef notdef
    if (hdr->flags & HBCC) {
	insert_bcc(str);
	return;
    }
#endif	/* notdef */

    if (*str != '\n' && *str != '\0') {
	if (aliasflg && hdr->flags & HTRY) {
	    /* this header contains address(es) that we have to do
	     * alias expansion on.  Because of the saved state in
	     * getname we have to put all the addresses into a list.
	     * We then let putadr munch on that list, possibly
	     * expanding aliases.
	     */
	    register struct mailname *f = 0;
	    register struct mailname *mp = 0;

	    while ((cp = getname(str))) {
		mp = getm( cp, NULL, 0, AD_HOST, NULL);
		if (f == 0) {
		    f = mp;
		    mp->m_next = mp;
		} else {
		    mp->m_next = f->m_next;
		    f->m_next = mp;
		    f = mp;
		}
	    }
	    f = mp->m_next; mp->m_next = 0;
	    putadr( name, f );
	} else {
	    fprintf (out, "%s: %s", name, str );
	}
    }
}


static void
start_headers (void)
{
    char *cp;
    char sigbuf[BUFSIZ];

    strncpy(from, getusername(), sizeof(from));

    if ((cp = getfullname ()) && *cp) {
	strncpy (sigbuf, cp, sizeof(sigbuf));
	snprintf (signature, sizeof(signature), "%s <%s>", sigbuf,  from);
    }
    else
	snprintf (signature, sizeof(signature), "%s",  from);
}


static void
finish_headers (FILE *out)
{
    switch (msgstate) {
	case normal: 
	    if (!(msgflags & MDAT))
		fprintf (out, "Date: %s\n", dtimenow (0));
	    if (msgflags & MFRM)
		fprintf (out, "Sender: %s\n", from);
	    else
		fprintf (out, "From: %s\n", signature);
#ifdef notdef
	    if (!(msgflags & MVIS))
		fprintf (out, "Bcc: Blind Distribution List: ;\n");
#endif	/* notdef */
	    break;

	case resent: 
	    if (!(msgflags & MRDT))
		fprintf (out, "Resent-Date: %s\n", dtimenow(0));
	    if (msgflags & MRFM)
		fprintf (out, "Resent-Sender: %s\n", from);
	    else
		fprintf (out, "Resent-From: %s\n", signature);
#ifdef notdef
	    if (!(msgflags & MVIS))
		fprintf (out, "Resent-Bcc: Blind Re-Distribution List: ;\n");
#endif	/* notdef */
	    break;
    }

    if (badmsg)
	adios (NULL, "re-format message and try again");
}


static int
get_header (char *header, struct headers *table)
{
    struct headers *h;

    for (h = table; h->value; h++)
	if (!strcasecmp (header, h->value))
	    return (h - table);

    return NOTOK;
}


/*
 * output the address list for header "name".  The address list
 * is a linked list of mailname structs.  "nl" points to the head
 * of the list.  Alias substitution should be done on nl.
 */
static void
putadr (char *name, struct mailname *nl)
{
    register struct mailname *mp, *mp2;
    register int linepos;
    register char *cp;
    int namelen;

    fprintf (out, "%s: ", name);
    namelen = strlen(name) + 2;
    linepos = namelen;

    for (mp = nl; mp; ) {
	if (linepos > MAX_SM_FIELD) {
		fprintf (out, "\n%s: ", name);
		linepos = namelen;
	}
	if (mp->m_nohost) {
	    /* a local name - see if it's an alias */
	    cp = akvalue(mp->m_mbox);
	    if (cp == mp->m_mbox)
		/* wasn't an alias - use what the user typed */
		linepos = putone( mp->m_text, linepos, namelen );
	    else
		/* an alias - expand it */
		while ((cp = getname(cp))) {
		    if (linepos > MAX_SM_FIELD) {
			    fprintf (out, "\n%s: ", name);
			    linepos = namelen;
		    }
		    mp2 = getm( cp, NULL, 0, AD_HOST, NULL);
		    if (akvisible()) {
			mp2->m_pers = getcpy(mp->m_mbox);
			linepos = putone( adrformat(mp2), linepos, namelen );
		    } else {
			linepos = putone( mp2->m_text, linepos, namelen );
		    }
		    mnfree( mp2 );
		}
	} else {
	    /* not a local name - use what the user typed */
	    linepos = putone( mp->m_text, linepos, namelen );
	}
	mp2 = mp;
	mp = mp->m_next;
	mnfree( mp2 );
    }
    putc( '\n', out );
}

static int
putone (char *adr, int pos, int indent)
{
    register int len;
    static int linepos;

    len = strlen( adr );
    if (pos == indent)
	linepos = pos;
    else if ( linepos+len > outputlinelen ) {
	fprintf ( out, ",\n%*s", indent, "");
	linepos = indent;
	pos += indent + 2;
    }
    else {
	fputs( ", ", out );
	linepos += 2;
	pos += 2;
    }
    fputs( adr, out );

    linepos += len;
    return (pos+len);
}


static void
insert_fcc (struct headers *hdr, char *pp)
{
    char   *cp;

    for (cp = pp; isspace (*cp); cp++)
	continue;
    for (pp += strlen (pp) - 1; pp > cp && isspace (*pp); pp--)
	continue;
    if (pp >= cp)
	*++pp = 0;
    if (*cp == 0)
	return;

    if (fccind >= FCCS)
	adios (NULL, "too many %ss", hdr->value);
    fccfold[fccind++] = getcpy (cp);
}

#if 0
/* BCC GENERATION */

static void
make_bcc_file (void)
{
    pid_t child_id;
    int fd, i, status;
    char *vec[6];
    FILE * in, *out;

    mktemp (bccfil);
    if ((out = fopen (bccfil, "w")) == NULL)
	adios (bccfil, "unable to create");
    chmod (bccfil, 0600);

    fprintf (out, "Date: %s\n", dtimenow (0));
    fprintf (out, "From: %s\n", signature);
    if (subject)
	fprintf (out, "Subject: %s", subject);
    fprintf (out, "BCC:\n\n------- Blind-Carbon-Copy\n\n");
    fflush (out);

    if (filter == NULL) {
	if ((fd = open (tmpfil, O_RDONLY)) == NOTOK)
	    adios (NULL, "unable to re-open");
	cpydgst (fd, fileno (out), tmpfil, bccfil);
	close (fd);
    }
    else {
	vec[0] = r1bindex (mhlproc, '/');

	for (i = 0; (child_id = vfork()) == NOTOK && i < 5; i++)
	    sleep (5);
	switch (child_id) {
	    case NOTOK: 
		adios ("vfork", "unable to");

	    case OK: 
		dup2 (fileno (out), 1);

		i = 1;
		vec[i++] = "-forward";
		vec[i++] = "-form";
		vec[i++] = filter;
		vec[i++] = tmpfil;
		vec[i] = NULL;

		execvp (mhlproc, vec);
		adios (mhlproc, "unable to exec");

	    default: 
		if (status = pidwait(child_id, OK))
		    admonish (NULL, "%s lost (status=0%o)", vec[0], status);
		break;
	}
    }

    fseek (out, 0L, SEEK_END);
    fprintf (out, "\n------- End of Blind-Carbon-Copy\n");
    fclose (out);
}
#endif	/* if 0 */

/* FCC INTERACTION */

static void
file (char *path)
{
    int i;

    if (fccind == 0)
	return;

    for (i = 0; i < fccind; i++)
	if (whomflg)
	    printf ("Fcc: %s\n", fccfold[i]);
	else
	    fcc (path, fccfold[i]);
}


static void
fcc (char *file, char *folder)
{
    pid_t child_id;
    int i, status;
    char fold[BUFSIZ];

    if (verbose)
	printf ("%sFcc: %s\n", msgstate == resent ? "Resent-" : "", folder);
    fflush (stdout);

    for (i = 0; (child_id = vfork()) == NOTOK && i < 5; i++)
	sleep (5);
    switch (child_id) {
	case NOTOK: 
	    if (!verbose)
		fprintf (stderr, "  %sFcc %s: ",
			msgstate == resent ? "Resent-" : "", folder);
	    fprintf (verbose ? stdout : stderr, "no forks, so not ok\n");
	    break;

	case OK: 
	    snprintf (fold, sizeof(fold), "%s%s",
		    *folder == '+' || *folder == '@' ? "" : "+", folder);
	    execlp (fileproc, r1bindex (fileproc, '/'),
		    "-link", "-file", file, fold, NULL);
	    _exit (-1);

	default: 
	    if ((status = pidwait(child_id, OK))) {
		if (!verbose)
		    fprintf (stderr, "  %sFcc %s: ",
			    msgstate == resent ? "Resent-" : "", folder);
		fprintf (verbose ? stdout : stderr,
			" errored (0%o)\n", status);
	    }
    }

    fflush (stdout);
}


#if 0

/*
 * TERMINATION
 */

static void
die (char *what, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    advertise (what, NULL, fmt, ap);
    va_end(ap);

    done (1);
}
#endif
