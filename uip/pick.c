
/*
 * pick.c -- search for messages by content
 *
 * This code is Copyright (c) 2002, 2008, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/tws.h>
#include <h/picksbr.h>
#include <h/utils.h>

#define PICK_SWITCHES \
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
    int publicsw = -1, zerosw = 1, vecp = 0;
    size_t seqp = 0;
    int lo, hi, msgnum;
    char *maildir, *folder = NULL, buf[100];
    char *cp, **argp, **arguments;
    char *seqs[NUMATTRS + 1], *vec[MAXARGS];
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp;
    register FILE *fp;

    done=putzero_done;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

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
		adios (NULL, "-%s unknown", cp);

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
		    adios (NULL, "missing argument to %s", argp[-2]);
		vec[vecp++] = cp;
		continue;
	    case OTHRSW: 
		adios (NULL, "internal error!");

	    case ANDSW:
	    case ORSW:
	    case NOTSW:
	    case LBRSW:
	    case RBRSW:
		vec[vecp++] = --cp;
		continue;

	    case SEQSW: 
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);

		/* check if too many sequences specified */
		if (seqp >= NUMATTRS)
		    adios (NULL, "too many sequences (more than %d) specified", NUMATTRS);

                if (!seq_nameok (cp))
                  done (1);

		seqs[seqp++] = cp;
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
		zerosw++;
		continue;
	    case NZEROSW: 
		zerosw = 0;
		continue;

	    case LISTSW: 
		listsw = 1;
		continue;
	    case NLISTSW: 
		listsw = 0;
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
	adios (NULL, "unable to read folder %s", folder);

    /* check for empty folder */
    if (mp->nummsg == 0)
	adios (NULL, "no messages in %s", folder);

    /* parse all the message ranges/sequences and set SELECTED */
    for (msgnum = 0; msgnum < msgs.size; msgnum++)
	if (!m_convert (mp, msgs.msgs[msgnum]))
	    done (1);
    seq_setprev (mp);	/* set the previous-sequence */

    /*
     * If we aren't saving the results to a sequence,
     * we default to list the results.
     */
    if (listsw == -1)
	listsw = !seqp;

    if (publicsw == 1 && is_readonly(mp))
	adios (NULL, "folder %s is read-only, so -public not allowed", folder);

    if (!pcompile (vec, NULL))
	done (1);

    lo = mp->lowsel;
    hi = mp->hghsel;

    /* If printing message numbers to standard out, force line buffering on.
     */
    if (listsw)
	setvbuf (stdout, NULL, _IOLBF, 0);

    /*
     * Scan through all the SELECTED messages and check for a
     * match.  If the message does not match, then unselect it.
     */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (is_selected (mp, msgnum)) {
	    if ((fp = fopen (cp = m_name (msgnum), "r")) == NULL)
		admonish (cp, "unable to read message");
	    if (fp && pmatches (fp, msgnum, 0L, 0L)) {
		if (msgnum < lo)
		    lo = msgnum;
		if (msgnum > hi)
		    hi = msgnum;

		if (listsw)
		    printf ("%s\n", m_name (msgnum));
	    } else {
		/* if it doesn't match, then unselect it */
		unset_selected (mp, msgnum);
		mp->numsel--;
	    }
	    if (fp)
		fclose (fp);
	}
    }

    mp->lowsel = lo;
    mp->hghsel = hi;

    if (mp->numsel <= 0)
	adios (NULL, "no messages match specification");

    seqs[seqp] = NULL;

    /*
     * Add the matching messages to sequences
     */
    for (seqp = 0; seqs[seqp]; seqp++)
	if (!seq_addsel (mp, seqs[seqp], publicsw, zerosw))
	    done (1);

    /*
     * Print total matched if not printing each matched message number.
     */
    if (!listsw) {
	printf ("%d hit%s\n", mp->numsel, mp->numsel == 1 ? "" : "s");
    }

    context_replace (pfolder, folder);	/* update current folder         */
    seq_save (mp);			/* synchronize message sequences */
    context_save ();			/* save the context file         */
    folder_free (mp);			/* free folder/message structure */
    done (0);
    return 1;
}


static void
putzero_done (int status)
{
    if (listsw && status && !isatty (fileno (stdout)))
	printf ("0\n");
    exit (status);
}
