/* pick.c -- search for messages by content
 *
 * This code is Copyright (c) 2002, 2008, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/getarguments.h"
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
#include "sbr/ambigsw.h"
#include "sbr/path.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/seq_nameok.h"
#include "sbr/seq_add.h"
#include "sbr/error.h"
#include "h/tws.h"
#include "h/picksbr.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/m_maildir.h"

#define PICK_SWITCHES \
    X("reverse", 0, REVSW) \
    X("and", 0, ANDSW) \
    X("or", 0, ORSW) \
    X("not", 0, NOTSW) \
    X("lbrace", 0, LBRSW) \
    X("rbrace", 0, RBRSW) \
    X("cc  pattern", 0, CCSW) \
    X("date  pattern", 0, DATESW) \
    X("from  pattern", 0, FROMSW) \
    X("search  pattern", 0, SRCHSW) \
    X("subject  pattern", 0, SUBJSW) \
    X("to  pattern", 0, TOSW) \
    X("-othercomponent  pattern", 0, OTHRSW) \
    X("after date", 0, AFTRSW) \
    X("before date", 0, BEFRSW) \
    X("datefield field", 5, DATFDSW) \
    X("sequence name", 0, SEQSW) \
    X("nosequence", 0, NSEQSW) \
    X("public", 0, PUBLSW) \
    X("nopublic", 0, NPUBLSW) \
    X("zero", 0, ZEROSW) \
    X("nozero", 0, NZEROSW) \
    X("list", 0, LISTSW) \
    X("nolist", 0, NLISTSW) \
    X("debug", 0, DEBUGSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(PICK);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(PICK, switches);
#undef X

static int listsw = -1;

static void putzero_done (int) NORETURN;

int
main (int argc, char **argv)
{
    int publicsw = -1;
    bool zerosw = true;
    int vecp = 0;
    size_t seqp = 0;
    int msgnum;
    char *maildir, *folder = NULL, buf[100];
    char *cp, **argp, **arguments;
    svector_t seqs = svector_create (0);
    char *vec[MAXARGS];
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgnum_array nums = { 0, 0, NULL };
    struct msgs *mp, *mp2;
    FILE *fp;
    int debug = 0;
    bool reverse = false;
    int start, end, inc;

    if (nmh_init(argv[0], true, true)) { return 1; }

    set_done(putzero_done);

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    if (*++cp == '-') {
		vec[vecp++] = --cp;
		goto pattern;
	    }
	    switch (smatch (cp, switches)) {
	    case AMBIGSW: 
		ambigsw (cp, switches);
		listsw = 0;	/* HACK */
		done (1);
	    case UNKWNSW: 
		die("-%s unknown", cp);

	    case HELPSW: 
		snprintf (buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
			  invo_name);
		print_help (buf, switches, 1);
		listsw = 0;	/* HACK */
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		listsw = 0;	/* HACK */
		done (0);

            case REVSW:
                reverse = true;
                continue;

	    case CCSW: 
	    case DATESW: 
	    case FROMSW: 
	    case SUBJSW: 
	    case TOSW: 
	    case DATFDSW: 
	    case AFTRSW: 
	    case BEFRSW: 
	    case SRCHSW: 
		vec[vecp++] = --cp;
	    pattern:
		if (!(cp = *argp++))/* allow -xyz arguments */
		    die("missing argument to %s", argp[-2]);
		vec[vecp++] = cp;
		continue;
	    case OTHRSW: 
		die("internal error!");

	    case ANDSW:
	    case ORSW:
	    case NOTSW:
	    case LBRSW:
	    case RBRSW:
		vec[vecp++] = --cp;
		continue;

	    case SEQSW: 
		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument to %s", argp[-2]);

                if (!seq_nameok (cp))
                  done (1);

		svector_push_back (seqs, cp);
		seqp++;
		continue;
	    case NSEQSW:
		seqp = 0;
		continue;
	    case PUBLSW: 
		publicsw = 1;
		continue;
	    case NPUBLSW: 
		publicsw = 0;
		continue;
	    case ZEROSW: 
		zerosw = true;
		continue;
	    case NZEROSW: 
		zerosw = false;
		continue;

	    case LISTSW: 
		listsw = 1;
		continue;
	    case NLISTSW: 
		listsw = 0;
		continue;

	    case DEBUGSW:
		++debug;
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
    vec[vecp] = NULL;

    if (!context_find ("path"))
	free (path ("./", TFOLDER));

    /*
     * If we didn't specify which messages to search,
     * then search the whole folder.
     */
    if (!msgs.size)
	app_msgarg(&msgs, "all");

    if (!folder)
	folder = getfolder (1);
    maildir = m_maildir (folder);

    if (chdir (maildir) == NOTOK)
	adios (maildir, "unable to change directory to");

    /* read folder and create message structure */
    if (!(mp = folder_read (folder, 0)))
	die("unable to read folder %s", folder);

    /* check for empty folder */
    if (mp->nummsg == 0)
	die("no messages in %s", folder);

    /* parse all the message ranges/sequences and set SELECTED */
    for (msgnum = 0; msgnum < msgs.size; msgnum++)
	if (!m_convert (mp, msgs.msgs[msgnum]))
	    done (1);

    /*
     * If we aren't saving the results to a sequence,
     * we default to list the results.
     */
    if (listsw == -1)
	listsw = !seqp;

    if (publicsw == 1 && is_readonly(mp))
	die("folder %s is read-only, so -public not allowed", folder);

    if (!pcompile (vec, NULL))
	done (1);

    /* If printing message numbers to standard out, force line buffering on.
     */
    if (listsw)
	setvbuf (stdout, NULL, _IOLBF, 0);

    /*
     * Scan through all the SELECTED messages and check for a
     * match.  If there is NOT a match, then add it to a list to
     * remove from the final sequence (it will make sense later)
     */
    if (!reverse) { /* Overflow or underflow is fine. */
        start = mp->lowsel;
        end = mp->hghsel + 1;
        inc = 1;
    } else {
        start = mp->hghsel;
        end = mp->lowsel - 1;
        inc = -1;
    }
    for (msgnum = start; msgnum != end; msgnum += inc) {
	if (is_selected (mp, msgnum)) {
	    if ((fp = fopen (cp = m_name (msgnum), "r")) == NULL)
		admonish (cp, "unable to read message");
	    if (fp && pmatches (fp, msgnum, 0L, 0L, debug)) {
		if (listsw)
		    puts(m_name (msgnum));
	    } else {
	    	app_msgnum(&nums, msgnum);
	    }
	    if (fp)
		fclose (fp);
	}
    }

    if (nums.size >= mp->numsel)
	die("no messages match specification");

    /*
     * So, what's happening here?
     *
     * - Re-read the folder and sequences, but with locking.
     * - Recreate the original set of selected messages from the command line
     * - Set the previous sequence
     * - Remove any messages that did NOT result in hits from the selection
     * - Add to any new sequences
     */

    if (!(mp2 = folder_read (folder, 1)))
	die("unable to reread folder %s", folder);

    for (msgnum = 0; msgnum < msgs.size; msgnum++)
	if (!m_convert (mp2, msgs.msgs[msgnum]))
	    done (1);
    seq_setprev (mp2);	/* set the previous-sequence */

    /*
     * Nums contains a list of messages that we did NOT match.  Remove
     * that from the selection.
     */

    for (msgnum = 0; msgnum < nums.size; msgnum++) {
    	unset_selected(mp2, nums.msgnums[msgnum]);
	mp2->numsel--;
    }

    /*
     * Add the matching messages to sequences
     */
    if (seqp > 0) {
	for (seqp = 0; seqp < svector_size (seqs); seqp++)
	    if (!seq_addsel (mp2, svector_at (seqs, seqp), publicsw, zerosw))
		done (1);
    }

    /*
     * Print total matched if not printing each matched message number.
     */
    if (!listsw) {
	printf ("%d hit%s\n", mp2->numsel, PLURALS(mp2->numsel));
    }

    svector_free (seqs);
    context_replace (pfolder, folder);	/* update current folder         */
    seq_save (mp2);			/* synchronize message sequences */
    context_save ();			/* save the context file         */
    folder_free (mp);			/* free folder/message structure */
    folder_free (mp2);
    done (0);
    return 1;
}


static void NORETURN
putzero_done (int status)
{
    if (listsw && status && !isatty (fileno (stdout)))
	puts("0");
    exit (status);
}
