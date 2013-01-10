
/*
 * mhbuild.c -- expand/translate MIME composition files
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

#define MHBUILD_SWITCHES \
    X("check", 0, CHECKSW) \
    X("nocheck", 0, NCHECKSW) \
    X("directives", 0, DIRECTIVES) \
    X("nodirectives", 0, NDIRECTIVES) \
    X("headers", 0, HEADSW) \
    X("noheaders", 0, NHEADSW) \
    X("list", 0, LISTSW) \
    X("nolist", 0, NLISTSW) \
    X("realsize", 0, SIZESW) \
    X("norealsize", 0, NSIZESW) \
    X("rfc934mode", 0, RFC934SW) \
    X("norfc934mode", 0, NRFC934SW) \
    X("verbose", 0, VERBSW) \
    X("noverbose", 0, NVERBSW) \
    X("rcache policy", 0, RCACHESW) \
    X("wcache policy", 0, WCACHESW) \
    X("contentid", 0, CONTENTIDSW) \
    X("nocontentid", 0, NCONTENTIDSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("debug", -5, DEBUGSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MHBUILD);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MHBUILD, switches);
#undef X


/* mhbuildsbr.c */
extern char *tmp;	/* directory to place temp files */

/* mhcachesbr.c */
extern int rcachesw;
extern int wcachesw;
extern char *cache_public;
extern char *cache_private;

int debugsw = 0;
int verbosw = 0;

int listsw   = 0;
int rfc934sw = 0;
int contentidsw = 1;

/*
 * Temporary files
 */
static char infile[BUFSIZ];
static int unlink_infile  = 0;

static char outfile[BUFSIZ];
static int unlink_outfile = 0;

static void unlink_done (int) NORETURN;

/* mhbuildsbr.c */
CT build_mime (char *, int);
int output_message (CT, char *);
int output_message_fp (CT, FILE *, char*);

/* mhlistsbr.c */
int list_all_messages (CT *, int, int, int, int);

/* mhfree.c */
void free_content (CT);


int
main (int argc, char **argv)
{
    int sizesw = 1, headsw = 1, directives = 1;
    int *icachesw;
    char *cp, buf[BUFSIZ];
    char buffer[BUFSIZ], *compfile = NULL;
    char **argp, **arguments;
    CT ct, cts[2];
    FILE *fp = NULL;
    FILE *fp_out = NULL;

    done=unlink_done;

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
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

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

	    case HEADSW:
		headsw++;
		continue;
	    case NHEADSW:
		headsw = 0;
		continue;

	    case DIRECTIVES:
		directives = 1;
		continue;
	    case NDIRECTIVES:
		directives = 0;
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

	    case CONTENTIDSW:
		contentidsw = 1;
		continue;
	    case NCONTENTIDSW:
		contentidsw = 0;
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
	strncpy (infile, m_mktemp(invo_name, NULL, &fp), sizeof(infile));
	while (fgets (buffer, BUFSIZ, stdin))
	    fputs (buffer, fp);
	fclose (fp);
	unlink_infile = 1;

	/* build the content structures for MIME message */
	ct = build_mime (infile, directives);
	cts[0] = ct;
	cts[1] = NULL;

	/* output MIME message to this temporary file */
	strncpy (outfile, m_mktemp(invo_name, NULL, &fp_out), sizeof(outfile));
	unlink_outfile = 1;

	/* output the message */
	output_message_fp (ct, fp_out, outfile);
        fclose(fp_out);

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
    ct = build_mime (compfile, directives);
    cts[0] = ct;
    cts[1] = NULL;

    /* output MIME message to this temporary file */
    strncpy(outfile, m_mktemp2(compfile, invo_name, NULL, &fp_out),
            sizeof(outfile));
    unlink_outfile = 1;

    /* output the message */
    output_message_fp (ct, fp_out, outfile);
    fclose(fp_out);

    /*
     * List the message info
     */
    if (listsw)
	list_all_messages (cts, headsw, sizesw, verbosw, debugsw);

    /* Rename composition draft */
    snprintf (buffer, sizeof(buffer), "%s.orig", m_backup (compfile));
    if (rename (compfile, buffer) == NOTOK) {
	adios (compfile, "unable to rename comp draft %s to", buffer);
    }

    /* Rename output file to take its place */
    if (rename (outfile, compfile) == NOTOK) {
	advise (outfile, "unable to rename output %s to", compfile);
	rename (buffer, compfile);
	done (1);
    }
    unlink_outfile = 0;

    free_content (ct);
    done (0);
    return 1;
}


static void
unlink_done (int status)
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
