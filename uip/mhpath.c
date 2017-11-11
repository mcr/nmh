/* mhpath.c -- print full pathnames of nmh messages and folders
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/m_convert.h"
#include "sbr/getfolder.h"
#include "sbr/folder_read.h"
#include "sbr/folder_realloc.h"
#include "sbr/folder_free.h"
#include "sbr/context_save.h"
#include "sbr/context_find.h"
#include "sbr/ambigsw.h"
#include "sbr/path.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/error.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/m_maildir.h"

#define MHPATH_SWITCHES \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MHPATH);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MHPATH, switches);
#undef X

int
main(int argc, char **argv)
{
    int i;
    char *cp, *maildir, *folder = NULL;
    char **argp;
    char **arguments, buf[BUFSIZ];
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp;

    if (nmh_init(argv[0], true, false)) { return 1; }

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

    if (!folder)
	folder = getfolder (1);
    maildir = m_maildir (folder);

    /* If no messages are given, print folder pathname */
    if (!msgs.size) {
	puts(maildir);
	done (0);
    }

    if (chdir (maildir) == NOTOK)
	adios (maildir, "unable to change directory to");

    /* read folder and create message structure */
    if (!(mp = folder_read (folder, 1)))
	die("unable to read folder %s", folder);

    /*
     * We need to make sure there is message status space
     * for all the message numbers from 1 to "new" since
     * mhpath can select empty slots.  If we are adding
     * space at the end, we go ahead and add 10 slots.
     */
    if (mp->hghmsg >= mp->hghoff) {
	if (!(mp = folder_realloc (mp, 1, mp->hghmsg + 10)))
	    die("unable to allocate folder storage");
    } else if (mp->lowoff > 1) {
	if (!(mp = folder_realloc (mp, 1, mp->hghoff)))
	    die("unable to allocate folder storage");
    }

    mp->msgflags |= ALLOW_NEW;	/* allow the "new" sequence */

    /* parse all the message ranges/sequences and set SELECTED */
    for (i = 0; i < msgs.size; i++)
	if (!m_convert (mp, msgs.msgs[i]))
	    done (1);

    seq_setprev (mp);	/* set the previous-sequence */

    /* print the path of all selected messages */
    for (i = mp->lowsel; i <= mp->hghsel; i++)
	if (is_selected (mp, i))
	    printf ("%s/%s\n", mp->foldpath, m_name (i));

    seq_save (mp);	/* synchronize message sequences */
    context_save ();	/* save the context file         */
    folder_free (mp);	/* free folder/message structure */
    done (0);
    return 1;
}
