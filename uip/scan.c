
/*
 * scan.c -- display a one-line "scan" listing of folder or messages
 *
 * $Id$
 */

#include <h/mh.h>
#include <h/fmt_scan.h>
#include <h/scansbr.h>
#include <zotnet/tws/tws.h>
#include <errno.h>

/*
 * We allocate space for message names (msgs array)
 * this number of elements at a time.
 */
#define MAXMSGS  256


static struct swit switches[] = {
#define	CLRSW	0
    { "clear", 0 },
#define	NCLRSW	1
    { "noclear", 0 },
#define	FORMSW	2
    { "form formatfile", 0 },
#define	FMTSW	3
    { "format string", 5 },
#define	HEADSW	4
    { "header", 0 },
#define	NHEADSW	5
    { "noheader", 0 },
#define	WIDTHSW	6
    { "width columns", 0 },
#define	REVSW	7
    { "reverse", 0 },
#define	NREVSW	8
    { "noreverse", 0 },
#define	FILESW	9
    { "file file", 4 },
#define VERSIONSW 10
    { "version", 0 },
#define	HELPSW	11
    { "help", 4 },
    { NULL, 0 }
};

extern int errno;

/*
 * global for sbr/formatsbr.c - yech!
 */
#ifdef LBL
extern struct msgs *fmt_current_folder;	
#endif

/*
 * prototypes
 */
void clear_screen(void);  /* from termsbr.c */


int
main (int argc, char **argv)
{
    int clearflag = 0, hdrflag = 0, ontty;
    int width = 0, revflag = 0;
    int i, state, msgnum, nummsgs, maxmsgs;
    int seqnum[NUMATTRS], unseen, num_unseen_seq = 0;
    char *cp, *maildir, *file = NULL, *folder = NULL;
    char *form = NULL, *format = NULL, buf[BUFSIZ];
    char **argp, *nfs, **arguments, **msgs;
    struct msgs *mp;
    FILE *in;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    mts_init (invo_name);
    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /*
     * Allocate the initial space to record message
     * names, ranges, and sequences.
     */
    nummsgs = 0;
    maxmsgs = MAXMSGS;
    if (!(msgs = (char **) malloc ((size_t) (maxmsgs * sizeof(*msgs)))))
	adios (NULL, "unable to allocate storage");

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

		case CLRSW: 
		    clearflag++;
		    continue;
		case NCLRSW: 
		    clearflag = 0;
		    continue;

		case FORMSW: 
		    if (!(form = *argp++) || *form == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    format = NULL;
		    continue;
		case FMTSW: 
		    if (!(format = *argp++) || *format == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    form = NULL;
		    continue;

		case HEADSW: 
		    hdrflag++;
		    continue;
		case NHEADSW: 
		    hdrflag = 0;
		    continue;

		case WIDTHSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    width = atoi (cp);
		    continue;
		case REVSW:
		    revflag++;
		    continue;
		case NREVSW:
		    revflag = 0;
		    continue;

		case FILESW:
		    if (!(cp = *argp++) || (cp[0] == '-' && cp[1]))
			adios (NULL, "missing argument to %s", argp[-2]);
		    if (strcmp (file = cp, "-"))
			file = path (cp, TFILE);
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

    if (!context_find ("path"))
	free (path ("./", TFOLDER));

    /*
     * Get new format string.  Must be before chdir().
     */
    nfs = new_fs (form, format, FORMAT);

    /*
     * We are scanning a maildrop file
     */
    if (file) {
	if (nummsgs)
	    adios (NULL, "\"msgs\" not allowed with -file");
	if (folder)
	    adios (NULL, "\"+folder\" not allowed with -file");

	/* check if "file" is really stdin */
	if (strcmp (file, "-") == 0) {
	    in = stdin;
	    file = "stdin";
	} else {
	    if ((in = fopen (file, "r")) == NULL)
		adios (file, "unable to open");
	}

#ifndef	JLR
	if (hdrflag) {
	    printf ("FOLDER %s\t%s\n", file, dtimenow (1));
	}
#endif /* JLR */

	m_unknown (in);
	for (msgnum = 1; ; ++msgnum) {
	    state = scan (in, msgnum, -1, nfs, width, 0, 0,
		    hdrflag ? file : NULL, 0L, 1);
	    if (state != SCNMSG && state != SCNENC)
		break;
	}
	fclose (in);
	done (0);
    }

    /*
     * We are scanning a folder
     */

    if (!nummsgs)
	msgs[nummsgs++] = "all";
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
	    done(1);
    seq_setprev (mp);			/* set the Previous-Sequence */

    context_replace (pfolder, folder);	/* update current folder         */
    seq_save (mp);			/* synchronize message sequences */
    context_save ();			/* save the context file         */

    /*
     * Get the sequence number for each sequence
     * specified by Unseen-Sequence
     */
    if ((cp = context_find (usequence)) && *cp) {
	char **ap, *dp;

	dp = getcpy(cp);
	ap = brkstring (dp, " ", "\n");
	for (i = 0; ap && *ap; i++, ap++)
	    seqnum[i] = seq_getnum (mp, *ap);

	num_unseen_seq = i;
	if (dp)
	    free(dp);
    }

    ontty = isatty (fileno (stdout));

#ifdef LBL
    else
	fmt_current_folder = mp;
#endif

    for (msgnum = revflag ? mp->hghsel : mp->lowsel;
	 (revflag ? msgnum >= mp->lowsel : msgnum <= mp->hghsel);
	 msgnum += (revflag ? -1 : 1)) {
	if (is_selected(mp, msgnum)) {
	    if ((in = fopen (cp = m_name (msgnum), "r")) == NULL) {
#if 0
		if (errno != EACCES)
#endif
		    admonish (cp, "unable to open message");
#if 0
		else
		    printf ("%*d  unreadable\n", DMAXFOLDER, msgnum);
#endif
		continue;
	    }

#ifndef JLR
	    if (hdrflag) {
		printf ("FOLDER %s\t%s\n", folder, dtimenow(1));
	    }
#endif /* JLR */

	    /*
	     * Check if message is in any sequence given
	     * by Unseen-Sequence profile entry.
	     */
	    unseen = 0;
	    for (i = 0; i < num_unseen_seq; i++) {
		if (in_sequence(mp, seqnum[i], msgnum)) {
		    unseen = 1;
		    break;
		}
	    }

	    switch (state = scan (in, msgnum, 0, nfs, width,
			msgnum == mp->curmsg, unseen,
			hdrflag ? folder : NULL, 0L, 1)) {
		case SCNMSG: 
		case SCNENC: 
		case SCNERR: 
		    break;

		default: 
		    adios (NULL, "scan() botch (%d)", state);

		case SCNEOF: 
#if 0
		    printf ("%*d  empty\n", DMAXFOLDER, msgnum);
#else
		    advise (NULL, "message %d: empty", msgnum);
#endif
		    break;
	    }
	    hdrflag = 0;
	    fclose (in);
	    if (ontty)
		fflush (stdout);
	}
    }

#ifdef LBL
    seq_save (mp);	/* because formatsbr might have made changes */
#endif

    folder_free (mp);	/* free folder/message structure */
    if (clearflag)
	clear_screen ();

    return done (0);
}
