/* mhstore.c -- store the contents of MIME messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/m_name.h"
#include "sbr/m_gmprot.h"
#include "sbr/getarguments.h"
#include "sbr/seq_setprev.h"
#include "sbr/seq_setcur.h"
#include "sbr/seq_save.h"
#include "sbr/smatch.h"
#include "sbr/m_convert.h"
#include "sbr/getfolder.h"
#include "sbr/folder_read.h"
#include "sbr/context_save.h"
#include "sbr/context_replace.h"
#include "sbr/context_find.h"
#include "sbr/readconfig.h"
#include "sbr/ambigsw.h"
#include "sbr/path.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/error.h"
#include <fcntl.h>
#include "h/signals.h"
#include "h/mts.h"
#include "h/tws.h"
#include "h/mime.h"
#include "h/mhparse.h"
#include "h/mhcachesbr.h"
#include "h/done.h"
#include "h/utils.h"
#include "mhmisc.h"
#include "sbr/m_maildir.h"
#include "mhfree.h"

#define MHSTORE_SWITCHES \
    X("auto", 0, AUTOSW) \
    X("noauto", 0, NAUTOSW) \
    X("check", -5, CHECKSW) \
    X("nocheck", -7, NCHECKSW) \
    X("verbose", 0, VERBSW) \
    X("noverbose", 0, NVERBSW) \
    X("file file", 0, FILESW)		/* interface from show */ \
    X("outfile outfile", 0, OUTFILESW) \
    X("part number", 0, PARTSW) \
    X("type content", 0, TYPESW) \
    X("prefer content", 0, PREFERSW) \
    X("noprefer", 0, NPREFERSW) \
    X("rcache policy", 0, RCACHESW) \
    X("wcache policy", 0, WCACHESW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("clobber always|auto|suffix|ask|never", 0, CLOBBERSW) \
    X("debug", -5, DEBUGSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MHSTORE);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MHSTORE, switches);
#undef X


#define	quitser	pipeser

/* mhparse.c */
int debugsw = 0;

/*
 * static prototypes
 */
static void pipeser (int);


int
main (int argc, char **argv)
{
    int msgnum, *icachesw;
    bool autosw = false;
    /* verbosw defaults to 1 for backward compatibility. */
    bool verbosw = true;
    const char *clobbersw = "always";
    char *cp, *file = NULL, *outfile = NULL, *folder = NULL;
    char *maildir, buf[100], **argp;
    char **arguments;
    char *cwd;
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp = NULL;
    CT ct, *ctp;
    FILE *fp;
    int files_not_clobbered;
    mhstoreinfo_t info;

    if (nmh_init(argv[0], true, true)) { return 1; }

    set_done(freects_done);

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
		die("-%s unknown", cp);

	    case HELPSW:
		snprintf (buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
			invo_name);
		print_help (buf, switches, 1);
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case AUTOSW:
		autosw = true;
		continue;
	    case NAUTOSW:
		autosw = false;
		continue;

	    case RCACHESW:
		icachesw = &rcachesw;
		goto do_cache;
	    case WCACHESW:
		icachesw = &wcachesw;
do_cache:
		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument to %s", argp[-2]);
		switch (*icachesw = smatch (cp, cache_policy)) {
		case AMBIGSW:
		    ambigsw (cp, cache_policy);
		    done (1);
		case UNKWNSW:
		    die("%s unknown", cp);
		default:
		    break;
		}
		continue;

	    case CHECKSW:
	    case NCHECKSW:
		/* Currently a NOP */
		continue;

	    case PARTSW:
		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument to %s", argp[-2]);
		if (npart >= NPARTS)
		    die("too many parts (starting with %s), %d max",
			   cp, NPARTS);
		parts[npart++] = cp;
		continue;

	    case TYPESW:
		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument to %s", argp[-2]);
		if (ntype >= NTYPES)
		    die("too many types (starting with %s), %d max",
			   cp, NTYPES);
		types[ntype++] = cp;
		continue;

	    case PREFERSW:
		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument to %s", argp[-2]);
		if (npreferred >= NPREFS)
		    die("too many preferred types (starting with %s), %d max",
			   cp, NPREFS);
		mime_preference[npreferred].type = cp;
		cp = strchr(cp, '/');
		if (cp) *cp++ = '\0';
		mime_preference[npreferred++].subtype = cp;
		continue;

	    case NPREFERSW:
		npreferred = 0;
		continue;

	    case FILESW:
		if (!(cp = *argp++) || (*cp == '-' && cp[1]))
		    die("missing argument to %s", argp[-2]);
		file = *cp == '-' ? cp : path (cp, TFILE);
		continue;

	    case OUTFILESW:
		if (!(cp = *argp++) || (*cp == '-' && cp[1]))
		    die("missing argument to %s", argp[-2]);
		outfile = *cp == '-' ? cp : path (cp, TFILE);
		continue;

	    case VERBSW:
		verbosw = true;
		continue;
	    case NVERBSW:
		verbosw = false;
		continue;
            case CLOBBERSW:
		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument to %s", argp[-2]);
		clobbersw = cp;
		continue;
	    case DEBUGSW:
		debugsw = 1;
		continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		die("only one folder at a time!");
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
    if ((cp = getenv ("MHSTORE"))) {
	if ((fp = fopen (cp, "r"))) {
	    readconfig(NULL, fp, cp, 0);
	    fclose (fp);
	} else {
	    admonish ("", "unable to read $MHSTORE profile (%s)", cp);
	}
    }

    /*
     * Read the standard profile setup
     */
    if ((fp = fopen (cp = etcpath ("mhn.defaults"), "r"))) {
	readconfig(NULL, fp, cp, 0);
	fclose (fp);
    }

    /* Check for public cache location */
    if ((cache_public = context_find (nmhcache)) && *cache_public != '/')
	cache_public = NULL;

    /* Check for private cache location */
    if (!(cache_private = context_find (nmhprivcache)))
	cache_private = ".cache";
    cache_private = mh_xstrdup(m_maildir(cache_private));

    /*
     * Cache the current directory before we do any chdirs()'s.
     */
    cwd = mh_xstrdup(pwd());

    if (!context_find ("path"))
	free (path ("./", TFOLDER));

    if (file && msgs.size)
	die("cannot specify msg and file at same time!");

    /*
     * check if message is coming from file
     */
    if (file) {
	cts = mh_xcalloc(2, sizeof *cts);
	ctp = cts;

	if ((ct = parse_mime (file))) {
	    *ctp++ = ct;
	    if (outfile) {
		ct->c_storage = mh_xstrdup(outfile);
	    }
        }
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
	    die("unable to read folder %s", folder);

	/* check for empty folder */
	if (mp->nummsg == 0)
	    die("no messages in %s", folder);

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
		if ((ct = parse_mime (msgnam))) {
		    *ctp++ = ct;
		    if (outfile) {
			ct->c_storage = mh_xstrdup(outfile);
		    }
                }
	    }
	}
    }

    if (!*cts)
	done (1);

    userrs = true;
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
     * Store the message content
     */
    info = mhstoreinfo_create (cts, cwd, clobbersw, autosw, verbosw);
    store_all_messages (info);
    files_not_clobbered = mhstoreinfo_files_not_clobbered(info);
    mhstoreinfo_free(info);

    /* Now free all the structures for the content */
    for (ctp = cts; *ctp; ctp++)
	free_content (*ctp);

    free (cts);
    cts = NULL;

    /* If reading from a folder, do some updating */
    if (mp) {
	context_replace (pfolder, folder);/* update current folder  */
	seq_setcur (mp, mp->hghsel);	  /* update current message */
	seq_save (mp);			  /* synchronize sequences  */
	context_save ();		  /* save the context file  */
    }

    done (files_not_clobbered);
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
