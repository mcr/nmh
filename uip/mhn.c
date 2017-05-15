/* mhn.c -- display, list, cache, or store the contents of MIME messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/signals.h>
#include <h/md5.h>
#include <h/mts.h>
#include <h/tws.h>
#include <h/fmt_scan.h>
#include <h/mime.h>
#include <h/mhparse.h>
#include <h/mhcachesbr.h>
#include <h/utils.h>
#include "../sbr/m_maildir.h"
#include "mhfree.h"
#include "mhshowsbr.h"

#define MHN_SWITCHES \
    X("auto", 0, AUTOSW) \
    X("noauto", 0, NAUTOSW) \
    X("cache", 0, CACHESW) \
    X("nocache", 0, NCACHESW) \
    X("check", 0, CHECKSW) \
    X("nocheck", 0, NCHECKSW) \
    X("headers", 0, HEADSW) \
    X("noheaders", 0, NHEADSW) \
    X("list", 0, LISTSW) \
    X("nolist", 0, NLISTSW) \
    X("realsize", 0, SIZESW) \
    X("norealsize", 0, NSIZESW) \
    X("show", 0, SHOWSW) \
    X("noshow", 0, NSHOWSW) \
    X("store", 0, STORESW) \
    X("nostore", 0, NSTORESW) \
    X("verbose", 0, VERBSW) \
    X("noverbose", 0, NVERBSW) \
    X("file file", 0, FILESW) \
    X("form formfile", 0, FORMSW) \
    X("part number", 0, PARTSW) \
    X("type content", 0, TYPESW) \
    X("rcache policy", 0, RCACHESW) \
    X("wcache policy", 0, WCACHESW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    /*					\
     * for debugging			\
     */					\
    X("debug", -5, DEBUGSW) \
    /*					\
     * switches for moreproc/mhlproc	\
     */					\
    X("moreproc program", -4, PROGSW) \
    X("nomoreproc", -3, NPROGSW) \
    X("length lines", -4, LENSW) \
    X("width columns", -4, WIDTHSW) \
    /*					\
     * switches for mhbuild		\
     */					\
    X("build", -5, BUILDSW) \
    X("nobuild", -7, NBUILDSW) \
    X("rfc934mode", 0, RFC934SW) \
    X("norfc934mode", 0, NRFC934SW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MHN);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MHN, switches);
#undef X


/* mhmisc.c */
extern int npart;
extern int ntype;
extern char *parts[NPARTS + 1];
extern char *types[NTYPES + 1];
extern int userrs;

int debugsw = 0;
int verbosw = 0;

/*
 * variables for mhbuild (mhn -build)
 */
static int buildsw  = 0;
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
int part_ok (CT);
int type_ok (CT, int);
void flush_errors (void);

/*
 * static prototypes
 */
static void pipeser (int);


int
main (int argc, char **argv)
{
    int sizesw = 1, headsw = 1, autosw = 0;
    int msgnum, *icachesw;
    char *cp, *file = NULL, *folder = NULL;
    char *maildir, buf[100], **argp;
    char **arguments;
    char *cwd;
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp = NULL;
    CT ct, *ctp;
    FILE *fp;
    mhstoreinfo_t info;

    if (nmh_init(argv[0], 1)) { return 1; }

    done=freects_done;

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

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
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

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
		switch (*icachesw = smatch (cp, cache_policy)) {
		case AMBIGSW:
		    ambigsw (cp, cache_policy);
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
                mh_xfree(formsw);
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
		folder = pluspath (cp);
	} else
		app_msgarg(&msgs, cp);
    }

    /* null terminate the list of acceptable parts/types */
    parts[npart] = NULL;
    types[ntype] = NULL;

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
    cwd = mh_xstrdup(pwd());

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
	if (msgs.size < 1)
	    adios (NULL, "need to specify a %s composition file", invo_name);
	if (msgs.size > 1)
	    adios (NULL, "only one %s composition file at a time", invo_name);

	vecp = 0;
	vec[vecp++] = "mhbuild";

	if (rfc934sw == 1)
	    vec[vecp++] = "-rfc934mode";
	else if (rfc934sw == -1)
	    vec[vecp++] = "-norfc934mode";

	vec[vecp++] = msgs.msgs[0];
	vec[vecp] = NULL;

	execvp ("mhbuild", vec);
	fprintf (stderr, "unable to exec ");
	_exit (-1);
    }

    /*
     * Process a mhn composition file (old MH style)
     */
    if (msgs.size == 1 && !folder && !npart && !cachesw
	&& !showsw && !storesw && !ntype && !file
	&& (cp = getenv ("mhdraft"))
	&& strcmp (cp, msgs.msgs[0]) == 0) {

	char *vec[MAXARGS];
	int vecp;

	vecp = 0;
	vec[vecp++] = "mhbuild";

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

    if (file && msgs.size)
	adios (NULL, "cannot specify msg and file at same time!");

    /*
     * check if message is coming from file
     */
    if (file) {
	cts = mh_xcalloc(2, sizeof *cts);
	ctp = cts;

	if ((ct = parse_mime (file)))
	    *ctp++ = ct;
    } else {
	/*
	 * message(s) are coming from a folder
	 */
	if (!msgs.size)
	    app_msgarg(&msgs, "cur");
	if (!folder)
	    folder = getfolder (1);
	maildir = m_maildir (folder);

	if (chdir (maildir) == NOTOK)
	    adios (maildir, "unable to change directory to");

	/* read folder and create message structure */
	if (!(mp = folder_read (folder, 1)))
	    adios (NULL, "unable to read folder %s", folder);

	/* check for empty folder */
	if (mp->nummsg == 0)
	    adios (NULL, "no messages in %s", folder);

	/* parse all the message ranges/sequences and set SELECTED */
	for (msgnum = 0; msgnum < msgs.size; msgnum++)
	    if (!m_convert (mp, msgs.msgs[msgnum]))
		done (1);
	seq_setprev (mp);	/* set the previous-sequence */

	cts = mh_xcalloc(mp->numsel + 1, sizeof *cts);
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
	list_all_messages (cts, headsw, sizesw, verbosw, debugsw, 0);

    /*
     * Store the message content
     */
    if (storesw) {
	info = mhstoreinfo_create (cts, cwd, "always", autosw, verbosw);;
	store_all_messages (info);
	mhstoreinfo_free (info);
    }

    /* If reading from a folder, do some updating */
    if (mp) {
	context_replace (pfolder, folder);/* update current folder  */
	seq_setcur (mp, mp->hghsel);	  /* update current message */
	seq_save (mp);			  /* synchronize sequences  */
	context_save ();		  /* save the context file  */
    }

    /*
     * Cache the message content
     */
    if (cachesw)
	cache_all_messages (cts);

    /*
     * Show the message content
     */
    if (showsw)
	show_all_messages (cts, 0, 0, 0);

    /* Now free all the structures for the content */
    for (ctp = cts; *ctp; ctp++)
	free_content (*ctp);

    free (cts);
    cts = NULL;

    done (0);
    return 1;
}


static void
pipeser (int i)
{
    if (i == SIGQUIT) {
	fflush (stdout);
	fprintf (stderr, "\n");
	fflush (stderr);
    }

    done (1);
    /* NOTREACHED */
}
