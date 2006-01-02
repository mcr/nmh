
/*
 * mhn.c -- display, list, cache, or store the contents of MIME messages
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/signals.h>
#include <h/md5.h>
#include <errno.h>
#include <signal.h>
#include <h/mts.h>
#include <h/tws.h>
#include <h/mime.h>
#include <h/mhparse.h>
#include <h/mhcachesbr.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

/*
 * We allocate space for message names (msgs array)
 * this number of elements at a time.
 */
#define MAXMSGS  256


static struct swit switches[] = {
#define	AUTOSW                  0
    { "auto", 0 },
#define	NAUTOSW                 1
    { "noauto", 0 },
#define	CACHESW                 2
    { "cache", 0 },
#define	NCACHESW                3
    { "nocache", 0 },
#define	CHECKSW                 4
    { "check", 0 },
#define	NCHECKSW                5
    { "nocheck", 0 },
#define	HEADSW                  6
    { "headers", 0 },
#define	NHEADSW                 7
    { "noheaders", 0 },
#define	LISTSW                  8
    { "list", 0 },
#define	NLISTSW                 9
    { "nolist", 0 },
#define	PAUSESW                10
    { "pause", 0 },
#define	NPAUSESW               11
    { "nopause", 0 },
#define	SIZESW                 12
    { "realsize", 0 },
#define	NSIZESW                13
    { "norealsize", 0 },
#define	SERIALSW               14
    { "serialonly", 0 },
#define	NSERIALSW              15
    { "noserialonly", 0 },
#define	SHOWSW                 16
    { "show", 0 },
#define	NSHOWSW                17
    { "noshow", 0 },
#define	STORESW                18
    { "store", 0 },
#define	NSTORESW               19
    { "nostore", 0 },
#define	VERBSW                 20
    { "verbose", 0 },
#define	NVERBSW                21
    { "noverbose", 0 },
#define	FILESW                 22	/* interface from show */
    { "file file", 0 },
#define	FORMSW                 23
    { "form formfile", 0 },
#define	PARTSW                 24
    { "part number", 0 },
#define	TYPESW                 25
    { "type content", 0 },
#define	RCACHESW               26
    { "rcache policy", 0 },
#define	WCACHESW               27
    { "wcache policy", 0 },
#define VERSIONSW              28
    { "version", 0 },
#define	HELPSW                 29
    { "help", 0 },

/*
 * switches for debugging
 */
#define	DEBUGSW                30
    { "debug", -5 },

/*
 * switches for moreproc/mhlproc
 */
#define	PROGSW                 31
    { "moreproc program", -4 },
#define	NPROGSW                32
    { "nomoreproc", -3 },
#define	LENSW                  33
    { "length lines", -4 },
#define	WIDTHSW                34
    { "width columns", -4 },

/*
 * switches for mhbuild
 */
#define BUILDSW                35
    { "build", -5 },
#define NBUILDSW               36
    { "nobuild", -7 },
#define	EBCDICSW               37
    { "ebcdicsafe", -10 },
#define	NEBCDICSW              38
    { "noebcdicsafe", -12 },
#define	RFC934SW               39
    { "rfc934mode", -10 },
#define	NRFC934SW              40
    { "norfc934mode", -12 },
    { NULL, 0 }
};


/* mhparse.c */
extern int checksw;
extern char *tmp;	/* directory to place temp files */

/* mhcachesbr.c */
extern int rcachesw;
extern int wcachesw;
extern char *cache_public;
extern char *cache_private;

/* mhshowsbr.c */
extern int pausesw;
extern int serialsw;
extern char *progsw;
extern int nolist;
extern int nomore;	/* flags for moreproc/header display */
extern char *formsw;

/* mhstoresbr.c */
extern int autosw;
extern char *cwd;	/* cache current working directory */

/* mhmisc.c */
extern int npart;
extern int ntype;
extern char *parts[NPARTS + 1];
extern char *types[NTYPES + 1];
extern int userrs;

int debugsw = 0;
int verbosw = 0;

/* The list of top-level contents to display */
CT *cts = NULL;

/*
 * variables for mhbuild (mhn -build)
 */
static int buildsw  = 0;
static int ebcdicsw = 0;
static int rfc934sw = 0;

/*
 * what action to take?
 */
static int cachesw = 0;
static int listsw  = 0;
static int showsw  = 0;
static int storesw = 0;

#define	quitser	pipeser

/* mhparse.c */
CT parse_mime (char *);

/* mhmisc.c */
int part_ok (CT, int);
int type_ok (CT, int);
void set_endian (void);
void flush_errors (void);

/* mhshowsbr.c */
void show_all_messages (CT *);

/* mhlistsbr.c */
void list_all_messages (CT *, int, int, int, int);

/* mhstoresbr.c */
void store_all_messages (CT *);

/* mhcachesbr.c */
void cache_all_messages (CT *);

/* mhfree.c */
void free_content (CT);

/*
 * static prototypes
 */
static RETSIGTYPE pipeser (int);


int
main (int argc, char **argv)
{
    int sizesw = 1, headsw = 1;
    int nummsgs, maxmsgs, msgnum, *icachesw;
    char *cp, *file = NULL, *folder = NULL;
    char *maildir, buf[100], **argp;
    char **arguments, **msgs;
    struct msgs *mp = NULL;
    CT ct, *ctp;
    FILE *fp;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /*
     * Allocate the initial space to record message
     * names, ranges, and sequences.
     */
    nummsgs = 0;
    maxmsgs = MAXMSGS;
    msgs = (char **) mh_xmalloc ((size_t) (maxmsgs * sizeof(*msgs)));

    /*
     * Parse arguments
     */
    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
	    case AMBIGSW: 
		ambigsw (cp, switches);
		done (1);
	    case UNKWNSW: 
		adios (NULL, "-%s unknown", cp);

	    case HELPSW: 
		snprintf (buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
			invo_name);
		print_help (buf, switches, 1);
		done (1);
	    case VERSIONSW:
		print_version(invo_name);
		done (1);

	    case AUTOSW:
		autosw++;
		continue;
	    case NAUTOSW:
		autosw = 0;
		continue;

	    case CACHESW:
		cachesw++;
		continue;
	    case NCACHESW:
		cachesw = 0;
		continue;

	    case RCACHESW:
		icachesw = &rcachesw;
		goto do_cache;
	    case WCACHESW:
		icachesw = &wcachesw;
do_cache:
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		switch (*icachesw = smatch (cp, caches)) {
		case AMBIGSW:
		    ambigsw (cp, caches);
		    done (1);
		case UNKWNSW:
		    adios (NULL, "%s unknown", cp);
		default:
		    break;
		}
		continue;

	    case CHECKSW:
		checksw++;
		continue;
	    case NCHECKSW:
		checksw = 0;
		continue;

	    case HEADSW:
		headsw = 1;
		continue;
	    case NHEADSW:
		headsw = 0;
		continue;

	    case LISTSW:
		listsw = 1;
		continue;
	    case NLISTSW:
		listsw = 0;
		continue;

	    case PAUSESW:
		pausesw = 1;
		continue;
	    case NPAUSESW:
		pausesw = 0;
		continue;

	    case SERIALSW:
		serialsw = 1;
		continue;
	    case NSERIALSW:
		serialsw = 0;
		continue;

	    case SHOWSW:
		showsw = 1;
		continue;
	    case NSHOWSW:
		showsw = 0;
		continue;

	    case SIZESW:
		sizesw = 1;
		continue;
	    case NSIZESW:
		sizesw = 0;
		continue;

	    case STORESW:
		storesw = 1;
		continue;
	    case NSTORESW:
		storesw = 0;
		continue;

	    case PARTSW:
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		if (npart >= NPARTS)
		    adios (NULL, "too many parts (starting with %s), %d max",
			   cp, NPARTS);
		parts[npart++] = cp;
		continue;

	    case TYPESW:
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		if (ntype >= NTYPES)
		    adios (NULL, "too many types (starting with %s), %d max",
			   cp, NTYPES);
		types[ntype++] = cp;
		continue;

	    case FILESW:
		if (!(cp = *argp++) || (*cp == '-' && cp[1]))
		    adios (NULL, "missing argument to %s", argp[-2]);
		file = *cp == '-' ? cp : path (cp, TFILE);
		continue;

	    case FORMSW:
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		if (formsw)
		    free (formsw);
		formsw = getcpy (etcpath (cp));
		continue;

	    /*
	     * Switches for moreproc/mhlproc
	     */
	    case PROGSW:
		if (!(progsw = *argp++) || *progsw == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;
	    case NPROGSW:
		nomore++;
		continue;

	    case LENSW:
	    case WIDTHSW:
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;

	    /*
	     * Switches for mhbuild
	     */
	    case BUILDSW:
		buildsw = 1;
		continue;
	    case NBUILDSW:
		buildsw = 0;
		continue;
	    case RFC934SW:
		rfc934sw = 1;
		continue;
	    case NRFC934SW:
		rfc934sw = -1;
		continue;
	    case EBCDICSW:
		ebcdicsw = 1;
		continue;
	    case NEBCDICSW:
		ebcdicsw = -1;
		continue;

	    case VERBSW: 
		verbosw = 1;
		continue;
	    case NVERBSW: 
		verbosw = 0;
		continue;
	    case DEBUGSW:
		debugsw = 1;
		continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		adios (NULL, "only one folder at a time!");
	    else
		folder = path (cp + 1, *cp == '+' ? TFOLDER : TSUBCWF);
	} else {
	    /*
	     * Check if we need to allocate more space
	     * for message names/ranges/sequences.
	     */
	    if (nummsgs >= maxmsgs) {
		maxmsgs += MAXMSGS;
		if (!(msgs = (char **) realloc (msgs,
			(size_t) (maxmsgs * sizeof(*msgs)))))
		    adios (NULL, "unable to reallocate msgs storage");
	    }
	    msgs[nummsgs++] = cp;
	}
    }

    /* null terminate the list of acceptable parts/types */
    parts[npart] = NULL;
    types[ntype] = NULL;

    set_endian ();

    if ((cp = getenv ("MM_NOASK")) && !strcmp (cp, "1")) {
	nolist  = 1;
	listsw  = 0;
	pausesw = 0;
    }

    /*
     * Check if we've specified an additional profile
     */
    if ((cp = getenv ("MHN"))) {
	if ((fp = fopen (cp, "r"))) {
	    readconfig ((struct node **) 0, fp, cp, 0);
	    fclose (fp);
	} else {
	    admonish ("", "unable to read $MHN profile (%s)", cp);
	}
    }

    /*
     * Read the standard profile setup
     */
    if ((fp = fopen (cp = etcpath ("mhn.defaults"), "r"))) {
	readconfig ((struct node **) 0, fp, cp, 0);
	fclose (fp);
    }

    /* Check for public cache location */
    if ((cache_public = context_find (nmhcache)) && *cache_public != '/')
	cache_public = NULL;

    /* Check for private cache location */
    if (!(cache_private = context_find (nmhprivcache)))
	cache_private = ".cache";
    cache_private = getcpy (m_maildir (cache_private));

    /*
     * Cache the current directory before we do any chdirs()'s.
     */
    cwd = getcpy (pwd());

    /*
     * Check for storage directory.  If specified,
     * then store temporary files there.  Else we
     * store them in standard nmh directory.
     */
    if ((cp = context_find (nmhstorage)) && *cp)
	tmp = concat (cp, "/", invo_name, NULL);
    else
	tmp = add (m_maildir (invo_name), NULL);

    if (!context_find ("path"))
	free (path ("./", TFOLDER));

    /*
     * Process a mhn composition file (mhn -build)
     */
    if (buildsw) {
	char *vec[MAXARGS];
	int vecp;

	if (showsw || storesw || cachesw)
	    adios (NULL, "cannot use -build with -show, -store, -cache");
	if (nummsgs < 1)
	    adios (NULL, "need to specify a %s composition file", invo_name);
	if (nummsgs > 1)
	    adios (NULL, "only one %s composition file at a time", invo_name);

	vecp = 0;
	vec[vecp++] = "mhbuild";

	if (ebcdicsw == 1)
	    vec[vecp++] = "-ebcdicsafe";
	else if (ebcdicsw == -1)
	    vec[vecp++] = "-noebcdicsafe";

	if (rfc934sw == 1)
	    vec[vecp++] = "-rfc934mode";
	else if (rfc934sw == -1)
	    vec[vecp++] = "-norfc934mode";

	vec[vecp++] = msgs[0];
	vec[vecp] = NULL;

	execvp ("mhbuild", vec);
	fprintf (stderr, "unable to exec ");
	_exit (-1);
    }

    /*
     * Process a mhn composition file (old MH style)
     */
    if (nummsgs == 1 && !folder && !npart && !cachesw
	&& !showsw && !storesw && !ntype && !file
	&& (cp = getenv ("mhdraft"))
	&& strcmp (cp, msgs[0]) == 0) {

	char *vec[MAXARGS];
	int vecp;

	vecp = 0;
	vec[vecp++] = "mhbuild";

	if (ebcdicsw == 1)
	    vec[vecp++] = "-ebcdicsafe";
	else if (ebcdicsw == -1)
	    vec[vecp++] = "-noebcdicsafe";

	if (rfc934sw == 1)
	    vec[vecp++] = "-rfc934mode";
	else if (rfc934sw == -1)
	    vec[vecp++] = "-norfc934mode";

	vec[vecp++] = cp;
	vec[vecp] = NULL;

	execvp ("mhbuild", vec);
	fprintf (stderr, "unable to exec ");
	_exit (-1);
    }

    if (file && nummsgs)
	adios (NULL, "cannot specify msg and file at same time!");

    /*
     * check if message is coming from file
     */
    if (file) {
	if (!(cts = (CT *) calloc ((size_t) 2, sizeof(*cts))))
	    adios (NULL, "out of memory");
	ctp = cts;

	if ((ct = parse_mime (file)));
	    *ctp++ = ct;
    } else {
	/*
	 * message(s) are coming from a folder
	 */
	if (!nummsgs)
	    msgs[nummsgs++] = "cur";
	if (!folder)
	    folder = getfolder (1);
	maildir = m_maildir (folder);

	if (chdir (maildir) == NOTOK)
	    adios (maildir, "unable to change directory to");

	/* read folder and create message structure */
	if (!(mp = folder_read (folder)))
	    adios (NULL, "unable to read folder %s", folder);

	/* check for empty folder */
	if (mp->nummsg == 0)
	    adios (NULL, "no messages in %s", folder);

	/* parse all the message ranges/sequences and set SELECTED */
	for (msgnum = 0; msgnum < nummsgs; msgnum++)
	    if (!m_convert (mp, msgs[msgnum]))
		done (1);
	seq_setprev (mp);	/* set the previous-sequence */

	if (!(cts = (CT *) calloc ((size_t) (mp->numsel + 1), sizeof(*cts))))
	    adios (NULL, "out of memory");
	ctp = cts;

	for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	    if (is_selected(mp, msgnum)) {
		char *msgnam;

		msgnam = m_name (msgnum);
		if ((ct = parse_mime (msgnam)))
		    *ctp++ = ct;
	    }
	}
    }

    if (!*cts)
	done (1);

    /*
     * You can't give more than one of these flags
     * at a time.
     */
    if (showsw + listsw + storesw + cachesw > 1)
	adios (NULL, "can only use one of -show, -list, -store, -cache at same time");

    /* If no action is specified, assume -show */
    if (!listsw && !showsw && !storesw && !cachesw)
	showsw = 1;

    userrs = 1;
    SIGNAL (SIGQUIT, quitser);
    SIGNAL (SIGPIPE, pipeser);

    /*
     * Get the associated umask for the relevant contents.
     */
    for (ctp = cts; *ctp; ctp++) {
	struct stat st;

	ct = *ctp;
	if (type_ok (ct, 1) && !ct->c_umask) {
	    if (stat (ct->c_file, &st) != NOTOK)
		ct->c_umask = ~(st.st_mode & 0777);
	    else
		ct->c_umask = ~m_gmprot();
	}
    }

    /*
     * List the message content
     */
    if (listsw)
	list_all_messages (cts, headsw, sizesw, verbosw, debugsw);

    /*
     * Store the message content
     */
    if (storesw)
	store_all_messages (cts);

    /*
     * Cache the message content
     */
    if (cachesw)
	cache_all_messages (cts);

    /*
     * Show the message content
     */
    if (showsw)
	show_all_messages (cts);

    /* Now free all the structures for the content */
    for (ctp = cts; *ctp; ctp++)
	free_content (*ctp);

    free ((char *) cts);
    cts = NULL;

    /* If reading from a folder, do some updating */
    if (mp) {
	context_replace (pfolder, folder);/* update current folder  */
	seq_setcur (mp, mp->hghsel);	  /* update current message */
	seq_save (mp);			  /* synchronize sequences  */
	context_save ();		  /* save the context file  */
    }

    return done (0);
}


static RETSIGTYPE
pipeser (int i)
{
    if (i == SIGQUIT) {
	unlink ("core");
	fflush (stdout);
	fprintf (stderr, "\n");
	fflush (stderr);
    }

    done (1);
    /* NOTREACHED */
}


int
done (int status)
{
    CT *ctp;

    if ((ctp = cts))
	for (; *ctp; ctp++)
	    free_content (*ctp);

    exit (status);
    return 1;  /* dead code to satisfy the compiler */
}
