
/*
 * mhlist.c -- list the contents of MIME messages
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
#include <h/utils.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

static struct swit switches[] = {
#define	CHECKSW                 0
    { "check", 0 },
#define	NCHECKSW                1
    { "nocheck", 0 },
#define	HEADSW                  2
    { "headers", 0 },
#define	NHEADSW                 3
    { "noheaders", 0 },
#define	SIZESW                  4
    { "realsize", 0 },
#define	NSIZESW                 5
    { "norealsize", 0 },
#define	VERBSW                  6
    { "verbose", 0 },
#define	NVERBSW                 7
    { "noverbose", 0 },
#define	FILESW                  8	/* interface from show */
    { "file file", 0 },
#define	PARTSW                  9
    { "part number", 0 },
#define	TYPESW                 10
    { "type content", 0 },
#define	RCACHESW               11
    { "rcache policy", 0 },
#define	WCACHESW               12
    { "wcache policy", 0 },
#define VERSIONSW              13
    { "version", 0 },
#define	HELPSW                 14
    { "help", 0 },

/*
 * switches for debugging
 */
#define	DEBUGSW                15
    { "debug", -5 },
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

/* mhmisc.c */
extern int npart;
extern int ntype;
extern char *parts[NPARTS + 1];
extern char *types[NTYPES + 1];
extern int userrs;

/*
 * This is currently needed to keep mhparse happy.
 * This needs to be changed.
 */
pid_t xpid  = 0;

int debugsw = 0;
int verbosw = 0;

#define	quitser	pipeser

/* mhparse.c */
CT parse_mime (char *);

/* mhmisc.c */
int part_ok (CT, int);
int type_ok (CT, int);
void set_endian (void);
void flush_errors (void);

/* mhlistsbr.c */
void list_all_messages (CT *, int, int, int, int);

/* mhfree.c */
void free_content (CT);
extern CT *cts;
void freects_done (int) NORETURN;

/*
 * static prototypes
 */
static RETSIGTYPE pipeser (int);


int
main (int argc, char **argv)
{
    int sizesw = 1, headsw = 1;
    int msgnum, *icachesw;
    char *cp, *file = NULL, *folder = NULL;
    char *maildir, buf[100], **argp;
    char **arguments;
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp = NULL;
    CT ct, *ctp;

    done=freects_done;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

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
		done (1);
	    case VERSIONSW:
		print_version(invo_name);
		done (1);

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

	    case SIZESW:
		sizesw = 1;
		continue;
	    case NSIZESW:
		sizesw = 0;
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

    set_endian ();

    /* Check for public cache location */
    if ((cache_public = context_find (nmhcache)) && *cache_public != '/')
	cache_public = NULL;

    /* Check for private cache location */
    if (!(cache_private = context_find (nmhprivcache)))
	cache_private = ".cache";
    cache_private = getcpy (m_maildir (cache_private));

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

    if (file && msgs.size)
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
	if (!msgs.size)
	    app_msgarg(&msgs, "cur");
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
	for (msgnum = 0; msgnum < msgs.size; msgnum++)
	    if (!m_convert (mp, msgs.msgs[msgnum]))
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
    list_all_messages (cts, headsw, sizesw, verbosw, debugsw);

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

    done (0);
    return 1;
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
