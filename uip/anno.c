
/*
 * anno.c -- annotate messages
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 *
 *	Four new options have been added: delete, list, number, and draft.
 *	Message header fields are used by the new MIME attachment code in
 *	the send command.  Adding features to generalize the anno command
 *	seemed to be a better approach than the creation of a new command
 *	whose features would overlap with those of the anno command.
 *
 *	The -draft option causes anno to operate on the current draft file
 *	instead of on a message sequence.
 *
 *	The -delete option deletes header elements that match the -component
 *	field name.  If -delete is used without the -text option, the first
 *	header field whose field name matches the component name is deleted.
 *	If the -delete is used with the -text option, and the -text argument
 *	begins with a /, the first header field whose field name matches the
 *	component name and whose field body matches the text is deleted.  If
 *	the -text argument does not begin with a /, then the text is assumed
 *	to be the last component of a path name, and the first header field
 *	whose field name matches the component name and a field body whose
 *	last path name component matches the text is deleted.  If the -delete
 *	option is used with the new -number option described below, the nth
 *	header field whose field name matches the component name is deleted.
 *	No header fields are deleted if none of the above conditions are met.
 *
 *	The -list option outputs the field bodies from each header field whose
 *	field name matches the component name, one per line.  If no -text
 *	option is specified, only the last path name component of each field
 *	body is output.  The entire field body is output if the -text option
 *	is used; the contents of the -text argument are ignored.  If the -list
 *	option is used in conjuction with the new -number option described
 *	below, each line is numbered starting with 1.  A tab separates the
 *	number from the field body.
 *
 *	The -number option works with both the -delete and -list options as
 *	described above.  The -number option takes an optional argument.  A
 *	value of 1 is assumed if this argument is absent.
 */

#include <h/mh.h>
#include <h/utils.h>

static struct swit switches[] = {
#define	COMPSW	0
    { "component field", 0 },
#define	INPLSW	1
    { "inplace", 0 },
#define	NINPLSW	2
    { "noinplace", 0 },
#define	DATESW	3
    { "date", 0 },
#define	NDATESW	4
    { "nodate", 0 },
#define	TEXTSW	5
    { "text body", 0 },
#define VERSIONSW 6
    { "version", 0 },
#define	HELPSW	7
    { "help", 0 },
#define	DRFTSW			 8
    { "draft", 2 },
#define	LISTSW			 9
    { "list", 1 },
#define	DELETESW		10
    { "delete", 2 },
#define	NUMBERSW		11
    { "number", 2 },
#define	APPENDSW		12
    { "append", 1 },
#define	PRESERVESW		13
    { "preserve", 1 },
#define	NOPRESERVESW		14
    { "nopreserve", 3 },
    { NULL, 0 }
};

/*
 * static prototypes
 */
static void make_comp (char **);


int
main (int argc, char **argv)
{
    int inplace = 1, datesw = 1;
    int msgnum;
    char *cp, *maildir, *comp = NULL;
    char *text = NULL, *folder = NULL, buf[BUFSIZ];
    char **argp, **arguments;
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp;
    int		append = 0;		/* append annotations instead of default prepend */
    int		delete = -2;		/* delete header element if set */
    char	*draft = (char *)0;	/* draft file name */
    int		isdf = 0;		/* return needed for m_draft() */
    int		list = 0;		/* list header elements if set */
    int		number = 0;		/* delete specific number of like elements if set */

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

		case COMPSW:
		    if (comp)
			adios (NULL, "only one component at a time!");
		    if (!(comp = *argp++) || *comp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case DATESW:
		    datesw++;
		    continue;
		case NDATESW:
		    datesw = 0;
		    continue;

		case INPLSW:
		    inplace++;
		    continue;
		case NINPLSW:
		    inplace = 0;
		    continue;

		case TEXTSW:
		    if (text)
			adios (NULL, "only one body at a time!");
		    if (!(text = *argp++) || *text == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case DELETESW:		/* delete annotations */
		    delete = 0;
		    continue;

		case DRFTSW:		/* draft message specified */
		    draft = "";
		    continue;

		case LISTSW:		/* produce a listing */
		    list = 1;
		    continue;

		case NUMBERSW:		/* number listing or delete by number */
		    if (number != 0)
			adios (NULL, "only one number at a time!");

		    if (argp - arguments == argc - 1 || **argp == '-')
			number = 1;
		    
		    else {
		    	if (strcmp(*argp, "all") == 0)
		    	    number = -1;

		    	else if (!(number = atoi(*argp)))
			    adios (NULL, "missing argument to %s", argp[-2]);

			argp++;
		    }

		    delete = number;
		    continue;

		case APPENDSW:		/* append annotations instead of default prepend */
		    append = 1;
		    continue;

		case PRESERVESW:	/* preserve access and modification times on annotated message */
		    annopreserve(1);
		    continue;

		case NOPRESERVESW:	/* don't preserve access and modification times on annotated message (default) */
		    annopreserve(0);
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
     *	We're dealing with the draft message instead of message numbers.
     *	Get the name of the draft and deal with it just as we do with
     *	message numbers below.
     */

    if (draft != (char *)0) {
	if (msgs.size != 0)
	    adios(NULL, "can only have message numbers or -draft.");

	draft = getcpy(m_draft(folder, (char *)0, 1, &isdf));

	make_comp(&comp);

	if (list)
	    annolist(draft, comp, text, number);
	else
	    annotate (draft, comp, text, inplace, datesw, delete, append);

	return (done(0));
    }

#ifdef UCI
    if (strcmp(invo_name, "fanno") == 0)	/* ugh! */
	datesw = 0;
#endif	/* UCI */

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
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

    make_comp (&comp);

    /* annotate all the SELECTED messages */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (is_selected(mp, msgnum)) {
	    if (list)
		annolist(m_name(msgnum), comp, text, number);
	    else
		annotate (m_name (msgnum), comp, text, inplace, datesw, delete, append);
	}
    }

    context_replace (pfolder, folder);	/* update current folder  */
    seq_setcur (mp, mp->lowsel);	/* update current message */
    seq_save (mp);	/* synchronize message sequences */
    folder_free (mp);	/* free folder/message structure */
    context_save ();	/* save the context file         */
    return done (0);
}

static void
make_comp (char **ap)
{
    register char *cp;
    char buffer[BUFSIZ];

    if (*ap == NULL) {
	printf ("Enter component name: ");
	fflush (stdout);

	if (fgets (buffer, sizeof buffer, stdin) == NULL)
	    done (1);
	*ap = trimcpy (buffer);
    }

    if ((cp = *ap + strlen (*ap) - 1) > *ap && *cp == ':')
	*cp = 0;
    if (strlen (*ap) == 0)
	adios (NULL, "null component name");
    if (**ap == '-')
	adios (NULL, "invalid component name %s", *ap);
    if (strlen (*ap) >= NAMESZ)
	adios (NULL, "too large component name %s", *ap);

    for (cp = *ap; *cp; cp++)
	if (!isalnum (*cp) && *cp != '-')
	    adios (NULL, "invalid component name %s", *ap);
}
