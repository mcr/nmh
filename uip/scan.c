/* scan.c -- display a one-line "scan" listing of folder or messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/seq_setprev.h"
#include "sbr/seq_save.h"
#include "sbr/smatch.h"
#include "sbr/m_convert.h"
#include "sbr/getfolder.h"
#include "sbr/folder_read.h"
#include "sbr/folder_free.h"
#include "sbr/context_save.h"
#include "sbr/context_replace.h"
#include "sbr/context_find.h"
#include "sbr/brkstring.h"
#include "sbr/ambigsw.h"
#include "sbr/path.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/seq_getnum.h"
#include "sbr/error.h"
#include "h/fmt_scan.h"
#include "h/scansbr.h"
#include "h/tws.h"
#include "h/mts.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/m_maildir.h"
#include "sbr/terminal.h"

#define SCAN_SWITCHES \
    X("clear", 0, CLRSW) \
    X("noclear", 0, NCLRSW) \
    X("form formatfile", 0, FORMSW) \
    X("format string", 5, FMTSW) \
    X("header", 0, HEADSW) \
    X("noheader", 0, NHEADSW) \
    X("width columns", 0, WIDTHSW) \
    X("reverse", 0, REVSW) \
    X("noreverse", 0, NREVSW) \
    X("file file", 4, FILESW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(SCAN);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(SCAN, switches);
#undef X


int
main (int argc, char **argv)
{
    bool clearflag = false;
    bool hdrflag = false;
    int ontty;
    int width = -1;
    bool revflag = false;
    int i, state, msgnum;
    ivector_t seqnum = ivector_create (0);
    bool unseen;
    int num_unseen_seq = 0;
    char *cp, *maildir, *file = NULL, *folder = NULL;
    char *form = NULL, *format = NULL, buf[BUFSIZ];
    char **argp, *nfs, **arguments;
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp;
    FILE *in;

    if (nmh_init(argv[0], true, true)) { return 1; }

    mts_init ();
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

		case CLRSW: 
		    clearflag = true;
		    continue;
		case NCLRSW: 
		    clearflag = false;
		    continue;

		case FORMSW: 
		    if (!(form = *argp++) || *form == '-')
			die("missing argument to %s", argp[-2]);
		    format = NULL;
		    continue;
		case FMTSW: 
		    if (!(format = *argp++) || *format == '-')
			die("missing argument to %s", argp[-2]);
		    form = NULL;
		    continue;

		case HEADSW: 
		    hdrflag = true;
		    continue;
		case NHEADSW: 
		    hdrflag = false;
		    continue;

		case WIDTHSW: 
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    width = atoi (cp);
		    continue;
		case REVSW:
		    revflag = true;
		    continue;
		case NREVSW:
		    revflag = false;
		    continue;

		case FILESW:
		    if (!(cp = *argp++) || (cp[0] == '-' && cp[1]))
			die("missing argument to %s", argp[-2]);
		    if (strcmp (file = cp, "-"))
			file = path (cp, TFILE);
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
	if (msgs.size)
	    die("\"msgs\" not allowed with -file");
	if (folder)
	    die("\"+folder\" not allowed with -file");

	/* check if "file" is really stdin */
	if (strcmp (file, "-") == 0) {
	    in = stdin;
	    file = "stdin";
	} else {
	    if ((in = fopen (file, "r")) == NULL)
		adios (file, "unable to open");
	}

	if (hdrflag) {
	    printf ("FOLDER %s\t%s\n", file, dtimenow (1));
	}

	scan_detect_mbox_style (in);
	for (msgnum = 1; ; ++msgnum) {
	    charstring_t scanl = NULL;

	    state = scan (in, msgnum, -1, nfs, width, 0, 0,
			  hdrflag ? file : NULL, 0L, 1, &scanl);
	    charstring_free (scanl);
	    if (state != SCNMSG && state != SCNENC)
		break;
	}
	scan_finished ();
	fclose (in);
	done (0);
    }

    /*
     * We are scanning a folder
     */

    if (!msgs.size)
	app_msgarg(&msgs, "all");
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

	dp = mh_xstrdup(cp);
	ap = brkstring (dp, " ", "\n");
	for (i = 0; ap && *ap; i++, ap++)
	    ivector_push_back (seqnum, seq_getnum (mp, *ap));

	num_unseen_seq = i;
        free(dp);
    }

    ontty = isatty (fileno (stdout));

    for (msgnum = revflag ? mp->hghsel : mp->lowsel;
	 (revflag ? msgnum >= mp->lowsel : msgnum <= mp->hghsel);
	 msgnum += (revflag ? -1 : 1)) {
	if (is_selected(mp, msgnum)) {
	    charstring_t scanl = NULL;

	    if ((in = fopen (cp = m_name (msgnum), "r")) == NULL) {
		    admonish (cp, "unable to open message");
		continue;
	    }

	    if (hdrflag) {
		printf ("FOLDER %s\t%s\n", folder, dtimenow(1));
	    }

	    /*
	     * Check if message is in any sequence given
	     * by Unseen-Sequence profile entry.
	     */
	    unseen = false;
	    for (i = 0; i < num_unseen_seq; i++) {
		if (in_sequence(mp, ivector_at (seqnum, i), msgnum)) {
		    unseen = true;
		    break;
		}
	    }

	    switch (state = scan (in, msgnum, 0, nfs, width,
			msgnum == mp->curmsg, unseen,
			folder, 0L, 1, &scanl)) {
		case SCNMSG: 
		case SCNENC: 
		case SCNERR: 
		    break;

		default: 
		    die("scan() botch (%d)", state);

		case SCNEOF: 
		    inform("message %d: empty", msgnum);
		    break;
	    }
	    charstring_free (scanl);
	    scan_finished ();
	    hdrflag = false;
	    fclose (in);
	    if (ontty)
		fflush (stdout);
	}
    }

    ivector_free (seqnum);
    folder_free (mp);	/* free folder/message structure */
    if (clearflag)
	nmh_clear_screen ();

    done (0);
    return 1;
}
