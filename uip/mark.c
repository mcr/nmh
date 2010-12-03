
/*
 * mark.c -- add message(s) to sequences in given folder
 *        -- delete messages (s) from sequences in given folder
 *        -- list sequences in given folder
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>

static struct swit switches[] = {
#define	ADDSW               0
    { "add", 0 },
#define	DELSW               1
    { "delete", 0 },
#define	LSTSW               2
    { "list", 0 },
#define	SEQSW               3
    { "sequence name", 0 },
#define	PUBLSW              4
    { "public", 0 },
#define	NPUBLSW             5
    { "nopublic", 0 },
#define	ZEROSW              6
    { "zero", 0 },
#define	NZEROSW             7
    { "nozero", 0 },
#define VERSIONSW           8
    { "version", 0 },
#define	HELPSW              9
    { "help", 0 },
#define	DEBUGSW            10
    { "debug", -5 },
    { NULL, 0 }
};

/*
 * static prototypes
 */
static void print_debug (struct msgs *);
static void seq_printdebug (struct msgs *);


int
main (int argc, char **argv)
{
    int addsw = 0, deletesw = 0, debugsw = 0;
    int listsw = 0, publicsw = -1, zerosw = 0;
    int seqp = 0, msgnum;
    char *cp, *maildir, *folder = NULL, buf[BUFSIZ];
    char **argp, **arguments;
    char *seqs[NUMATTRS + 1];
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp;

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
		adios (NULL, "-%s unknown\n", cp);

	    case HELPSW: 
		snprintf (buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
			  invo_name);
		print_help (buf, switches, 1);
		done (1);
	    case VERSIONSW:
		print_version(invo_name);
		done (1);

	    case ADDSW: 
		addsw++;
		deletesw = listsw = 0;
		continue;
	    case DELSW: 
		deletesw++;
		addsw = listsw = 0;
		continue;
	    case LSTSW: 
		listsw++;
		addsw = deletesw = 0;
		continue;

	    case SEQSW: 
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);

		/* check if too many sequences specified */
		if (seqp >= NUMATTRS)
		    adios (NULL, "too many sequences (more than %d) specified", NUMATTRS);
		seqs[seqp++] = cp;
		continue;

	    case PUBLSW: 
		publicsw = 1;
		continue;
	    case NPUBLSW: 
		publicsw = 0;
		continue;

	    case DEBUGSW: 
		debugsw++;
		continue;

	    case ZEROSW: 
		zerosw++;
		continue;
	    case NZEROSW: 
		zerosw = 0;
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

    /*
     * If we haven't specified -add, -delete, or -list,
     * then use -add if a sequence was specified, else
     * use -list.
     */
    if (!addsw && !deletesw && !listsw) {
	if (seqp)
	    addsw++;
	else
	    listsw++;
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (!msgs.size)
	app_msgarg(&msgs, listsw ? "all" :"cur");
    if (!folder)
	folder = getfolder (1);
    maildir = m_maildir (folder);

    if (chdir (maildir) == NOTOK)
	adios (maildir, "unable to change directory to");

    /* read folder and create message structure */
    if (!(mp = folder_read (folder)))
	adios (NULL, "unable to read folder %s", folder);

    /* print some general debugging info */
    if (debugsw)
	print_debug(mp);

    /* check for empty folder */
    if (mp->nummsg == 0)
	adios (NULL, "no messages in %s", folder);

    /* parse all the message ranges/sequences and set SELECTED */
    for (msgnum = 0; msgnum < msgs.size; msgnum++)
	if (!m_convert (mp, msgs.msgs[msgnum]))
	    done (1);

    if (publicsw == 1 && is_readonly(mp))
	adios (NULL, "folder %s is read-only, so -public not allowed", folder);

    /*
     * Make sure at least one sequence has been
     * specified if we are adding or deleting.
     */
    if (seqp == 0 && (addsw || deletesw))
	adios (NULL, "-%s requires at least one -sequence argument",
	       addsw ? "add" : "delete");
    seqs[seqp] = NULL;

    /* Adding messages to sequences */
    if (addsw) {
	for (seqp = 0; seqs[seqp]; seqp++)
	    if (!seq_addsel (mp, seqs[seqp], publicsw, zerosw))
		done (1);
    }

    /* Deleting messages from sequences */
    if (deletesw) {
	for (seqp = 0; seqs[seqp]; seqp++)
	    if (!seq_delsel (mp, seqs[seqp], publicsw, zerosw))
		done (1);
    }

    /* Listing messages in sequences */
    if (listsw) {
	if (seqp) {
	    /* print the sequences given */
	    for (seqp = 0; seqs[seqp]; seqp++)
		seq_print (mp, seqs[seqp]);
	} else {
	    /* else print them all */
	    seq_printall (mp);
	}

	/* print debugging info about SELECTED messages */
	if (debugsw)
	    seq_printdebug (mp);
    }

    seq_save (mp);			/* synchronize message sequences */
    context_replace (pfolder, folder);	/* update current folder         */
    context_save ();			/* save the context file         */
    folder_free (mp);			/* free folder/message structure */
    done (0);
    return 1;
}


/*
 * Print general debugging info
 */
static void
print_debug (struct msgs *mp)
{
    char buf[100];

    printf ("invo_name     = %s\n", invo_name);
    printf ("mypath        = %s\n", mypath);
    printf ("defpath       = %s\n", defpath);
    printf ("ctxpath       = %s\n", ctxpath);
    printf ("context flags = %s\n", snprintb (buf, sizeof(buf),
		(unsigned) ctxflags, DBITS));
    printf ("foldpath      = %s\n", mp->foldpath);
    printf ("folder flags  = %s\n\n", snprintb(buf, sizeof(buf),
		(unsigned) mp->msgflags, FBITS));
    printf ("lowmsg=%d hghmsg=%d nummsg=%d curmsg=%d\n",
	mp->lowmsg, mp->hghmsg, mp->nummsg, mp->curmsg);
    printf ("lowsel=%d hghsel=%d numsel=%d\n",
	mp->lowsel, mp->hghsel, mp->numsel);
    printf ("lowoff=%d hghoff=%d\n\n", mp->lowoff, mp->hghoff);
}


/*
 * Print debugging info about all the SELECTED
 * messages and the sequences they are in.
 */
static void
seq_printdebug (struct msgs *mp)
{
    int msgnum;
    char buf[100];

    printf ("\n");
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (is_selected (mp, msgnum))
	    printf ("%*d: %s\n", DMAXFOLDER, msgnum,
		snprintb (buf, sizeof(buf),
		(unsigned) mp->msgstats[msgnum - mp->lowoff], seq_bits (mp)));
    }
}
