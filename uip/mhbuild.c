
/*
 * mhbuild.c -- expand/translate MIME composition files
 *
 * $Id$
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/signals.h>
#include <h/md5.h>
#include <errno.h>
#include <signal.h>
#include <zotnet/mts/mts.h>
#include <zotnet/tws/tws.h>
#include <h/mime.h>
#include <h/mhparse.h>
#include <h/mhcachesbr.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

static struct swit switches[] = {
#define	CHECKSW                 0
    { "check", 0 },
#define	NCHECKSW                1
    { "nocheck", 0 },
#define	EBCDICSW                2
    { "ebcdicsafe", 0 },
#define	NEBCDICSW               3
    { "noebcdicsafe", 0 },
#define	HEADSW                  4
    { "headers", 0 },
#define	NHEADSW                 5
    { "noheaders", 0 },
#define	LISTSW                  6
    { "list", 0 },
#define	NLISTSW                 7
    { "nolist", 0 },
#define	SIZESW                  8
    { "realsize", 0 },
#define	NSIZESW                 9
    { "norealsize", 0 },
#define	RFC934SW               10
    { "rfc934mode", 0 },
#define	NRFC934SW              11
    { "norfc934mode", 0 },
#define	VERBSW                 12
    { "verbose", 0 },
#define	NVERBSW                13
    { "noverbose", 0 },
#define	RCACHESW               14
    { "rcache policy", 0 },
#define	WCACHESW               15
    { "wcache policy", 0 },
#define VERSIONSW              16
    { "version", 0 },
#define	HELPSW                 17
    { "help", 4 },
#define	DEBUGSW                18
    { "debug", -5 },
    { NULL, 0 }
};


extern int errno;

/* mhbuildsbr.c */
extern int checksw;
extern char *tmp;	/* directory to place temp files */

/* mhcachesbr.c */
extern int rcachesw;
extern int wcachesw;
extern char *cache_public;
extern char *cache_private;

int debugsw = 0;
int verbosw = 0;

int ebcdicsw = 0;
int listsw   = 0;
int rfc934sw = 0;

/*
 * Temporary files
 */
static char infile[BUFSIZ];
static int unlink_infile  = 0;

static char outfile[BUFSIZ];
static int unlink_outfile = 0;


/* mhbuildsbr.c */
CT build_mime (char *);
int output_message (CT, char *);

/* mhlistsbr.c */
int list_all_messages (CT *, int, int, int, int);

/* mhmisc.c */
void set_endian (void);

/* mhfree.c */
void free_content (CT);


int
main (int argc, char **argv)
{
    int sizesw = 1, headsw = 1;
    int *icachesw;
    char *cp, buf[BUFSIZ];
    char buffer[BUFSIZ], *compfile = NULL;
    char **argp, **arguments;
    CT ct, cts[2];
    FILE *fp;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (cp[0] == '-' && cp[1] == '\0') {
	    if (compfile)
		adios (NULL, "cannot specify both standard input and a file");
	    else
		compfile = cp;
	    listsw = 0;		/* turn off -list if using standard in/out */
	    verbosw = 0;	/* turn off -verbose listings */
	    break;
	}
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

	    case RCACHESW:
		icachesw = &rcachesw;
		goto do_cache;
	    case WCACHESW:
		icachesw = &wcachesw;
	    do_cache: ;
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

	    case EBCDICSW:
		ebcdicsw++;
		continue;
	    case NEBCDICSW:
		ebcdicsw = 0;
		continue;

	    case HEADSW:
		headsw++;
		continue;
	    case NHEADSW:
		headsw = 0;
		continue;

	    case LISTSW:
		listsw++;
		continue;
	    case NLISTSW:
		listsw = 0;
		continue;

	    case RFC934SW:
		rfc934sw++;
		continue;
	    case NRFC934SW:
		rfc934sw = 0;
		continue;

	    case SIZESW:
		sizesw++;
		continue;
	    case NSIZESW:
		sizesw = 0;
		continue;

	    case VERBSW: 
		verbosw++;
		continue;
	    case NVERBSW: 
		verbosw = 0;
		continue;
	    case DEBUGSW:
		debugsw = 1;
		continue;
	    }
	}
	if (compfile)
	    adios (NULL, "only one composition file allowed");
	else
	    compfile = cp;
    }

    set_endian ();

    if ((cp = getenv ("MM_NOASK")) && !strcmp (cp, "1"))
	listsw  = 0;

    /*
     * Check if we've specified an additional profile
     */
    if ((cp = getenv ("MHBUILD"))) {
	if ((fp = fopen (cp, "r"))) {
	    readconfig ((struct node **) 0, fp, cp, 0);
	    fclose (fp);
	} else {
	    admonish ("", "unable to read $MHBUILD profile (%s)", cp);
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
     * Check for storage directory.  If defined, we
     * will store temporary files there.  Else we
     * store them in standard nmh directory.
     */
    if ((cp = context_find (nmhstorage)) && *cp)
	tmp = concat (cp, "/", invo_name, NULL);
    else
	tmp = add (m_maildir (invo_name), NULL);

    if (!context_find ("path"))
	free (path ("./", TFOLDER));

    /* Check if we have a file to process */
    if (!compfile)
	adios (NULL, "need to specify a %s composition file", invo_name);

    /*
     * Process the composition file from standard input.
     */
    if (compfile[0] == '-' && compfile[1] == '\0') {

	/* copy standard input to temporary file */
	strncpy (infile, m_scratch ("", invo_name), sizeof(infile));
	if ((fp = fopen (infile, "w")) == NULL)
	    adios (infile, "unable to open");
	while (fgets (buffer, BUFSIZ, stdin))
	    fputs (buffer, fp);
	fclose (fp);
	unlink_infile = 1;

	/* build the content structures for MIME message */
	ct = build_mime (infile);
	cts[0] = ct;
	cts[1] = NULL;

	/* output MIME message to this temporary file */
	strncpy (outfile, m_scratch ("", invo_name), sizeof(outfile));
	unlink_outfile = 1;

	/* output the message */
	output_message (ct, outfile);

	/* output the temp file to standard output */
	if ((fp = fopen (outfile, "r")) == NULL)
	    adios (outfile, "unable to open");
	while (fgets (buffer, BUFSIZ, fp))
	    fputs (buffer, stdout);
	fclose (fp);

	unlink (infile);
	unlink_infile = 0;

	unlink (outfile);
	unlink_outfile = 0;

	free_content (ct);
	done (0);
    }

    /*
     * Process the composition file from a file.
     */

    /* build the content structures for MIME message */
    ct = build_mime (compfile);
    cts[0] = ct;
    cts[1] = NULL;

    /* output MIME message to this temporary file */
    strncpy (outfile, m_scratch (compfile, invo_name), sizeof(outfile));
    unlink_outfile = 1;

    /* output the message */
    output_message (ct, outfile);

    /*
     * List the message info
     */
    if (listsw)
	list_all_messages (cts, headsw, sizesw, verbosw, debugsw);

    /* Rename composition draft */
    snprintf (buffer, sizeof(buffer), "%s.orig", m_backup (compfile));
    if (rename (compfile, buffer) == NOTOK)
	adios (compfile, "unable to rename %s to", buffer);

    /* Rename output file to take its place */
    if (rename (outfile, compfile) == NOTOK) {
	advise (outfile, "unable to rename %s to", compfile);
	rename (buffer, compfile);
	done (1);
    }
    unlink_outfile = 0;

    free_content (ct);
    done (0);
    /* NOT REACHED */
}


void
done (int status)
{
    /*
     * Check if we need to remove stray
     * temporary files.
     */
    if (unlink_infile)
	unlink (infile);
    if (unlink_outfile)
	unlink (outfile);

    exit (status);
}
