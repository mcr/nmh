/* anno.c -- annotate messages
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
 *	option is used in conjunction with the new -number option described
 *	below, each line is numbered starting with 1.  A tab separates the
 *	number from the field body.
 *
 *	The -number option works with both the -delete and -list options as
 *	described above.  The -number option takes an optional argument.  A
 *	value of 1 is assumed if this argument is absent.
 */

#include "h/mh.h"
#include "sbr/folder_free.h"
#include "sbr/context_save.h"
#include "sbr/context_replace.h"
#include "sbr/context_find.h"
#include "sbr/ambigsw.h"
#include "sbr/path.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/error.h"
#include "h/utils.h"
#include "h/done.h"
#include "sbr/m_maildir.h"

#define ANNO_SWITCHES \
    X("component field", 0, COMPSW) \
    X("inplace", 0, INPLSW) \
    X("noinplace", 0, NINPLSW) \
    X("date", 0, DATESW) \
    X("nodate", 0, NDATESW) \
    X("text body", 0, TEXTSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("draft", 2, DRFTSW) \
    X("list", 1, LISTSW) \
    X("delete", 2, DELETESW) \
    X("number", 2, NUMBERSW) \
    X("append", 1, APPENDSW) \
    X("preserve", 1, PRESERVESW) \
    X("nopreserve", 3, NOPRESERVESW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(ANNO);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(ANNO, switches);
#undef X

/*
 * static prototypes
 */
static void make_comp (char **);


int
main (int argc, char **argv)
{
    bool inplace, datesw;
    int msgnum;
    char *cp, *maildir;
    char *comp = NULL, *text = NULL, *folder = NULL, buf[BUFSIZ];
    char **argp, **arguments;
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp;
    bool	append;			/* append annotations instead of default prepend */
    int		delete = -2;		/* delete header element if set */
    char	*draft = NULL;	/* draft file name */
    int		isdf = 0;		/* return needed for m_draft() */
    bool	list;			/* list header elements if set */
    int		number = 0;		/* delete specific number of like elements if set */

    if (nmh_init(argv[0], true, true)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    append = list = false;
    inplace = datesw = true;
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

		case COMPSW:
		    if (comp)
			die("only one component at a time!");
		    if (!(comp = *argp++) || *comp == '-')
			die("missing argument to %s", argp[-2]);
		    continue;

		case DATESW:
		    datesw = true;
		    continue;
		case NDATESW:
		    datesw = false;
		    continue;

		case INPLSW:
		    inplace = true;
		    continue;
		case NINPLSW:
		    inplace = false;
		    continue;

		case TEXTSW:
		    if (text)
			die("only one body at a time!");
		    if (!(text = *argp++) || *text == '-')
			die("missing argument to %s", argp[-2]);
		    continue;

		case DELETESW:		/* delete annotations */
		    delete = 0;
		    continue;

		case DRFTSW:		/* draft message specified */
		    draft = "";
		    continue;

		case LISTSW:		/* produce a listing */
		    list = true;
		    continue;

		case NUMBERSW:		/* number listing or delete by number */
		    if (number != 0)
			die("only one number at a time!");

		    if (argp - arguments == argc - 1 || **argp == '-')
			number = 1;
		    
		    else {
		    	if (strcmp(*argp, "all") == 0)
		    	    number = -1;

		    	else if (!(number = atoi(*argp)))
			    die("missing argument to %s", argp[-1]);

			argp++;
		    }

		    delete = number;
		    continue;

		case APPENDSW:		/* append annotations instead of default prepend */
		    append = true;
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
		die("only one folder at a time!");
            folder = pluspath (cp);
	} else
		app_msgarg(&msgs, cp);
    }

    /*
     *	We're dealing with the draft message instead of message numbers.
     *	Get the name of the draft and deal with it just as we do with
     *	message numbers below.
     */

    if (draft != NULL) {
	if (msgs.size != 0)
	    die("can only have message numbers or -draft.");

	draft = mh_xstrdup(m_draft(folder, NULL, 1, &isdf));

	make_comp(&comp);

	if (list)
	    annolist(draft, comp, text, number);
	else
	    annotate (draft, comp, text, inplace, datesw, delete, append);

	done(0);
	return 1;
    }

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
    if (!(mp = folder_read (folder, 1)))
	die("unable to read folder %s", folder);

    /* check for empty folder */
    if (mp->nummsg == 0)
	die("no messages in %s", folder);

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
    done (0);
    return 1;
}

static void
make_comp (char **ap)
{
    char *cp, buffer[BUFSIZ];

    if (*ap == NULL) {
	fputs("Enter component name: ", stdout);
	fflush (stdout);

	if (fgets (buffer, sizeof buffer, stdin) == NULL)
	    done (1);
	*ap = trimcpy (buffer);
    }

    if ((cp = *ap + strlen (*ap) - 1) > *ap && *cp == ':')
	*cp = 0;
    if (!**ap)
	die("null component name");
    if (**ap == '-')
	die("invalid component name %s", *ap);
    if (strlen (*ap) >= NAMESZ)
	die("too large component name %s", *ap);

    for (cp = *ap; *cp; cp++)
	if (!isalnum ((unsigned char) *cp) && *cp != '-')
	    die("invalid component name %s", *ap);
}
