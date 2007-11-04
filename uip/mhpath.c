
/*
 * mhpath.c -- print full pathnames of nmh messages and folders
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>

static struct swit switches[] = {
#define VERSIONSW 0
    { "version", 0 },
#define	HELPSW	1
    { "help", 0 },
    { NULL, 0 }
};

int
main(int argc, char **argv)
{
    int i;
    char *cp, *maildir, *folder = NULL;
    char **argp;
    char **arguments, buf[BUFSIZ];
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
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);
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

    if (!context_find ("path"))
	free (path ("./", TFOLDER));

    if (!folder)
	folder = getfolder (1);
    maildir = m_maildir (folder);

    /* If no messages are given, print folder pathname */
    if (!msgs.size) {
	printf ("%s\n", maildir);
	done (0);
    }

    if (chdir (maildir) == NOTOK)
	adios (maildir, "unable to change directory to");

    /* read folder and create message structure */
    if (!(mp = folder_read (folder)))
	adios (NULL, "unable to read folder %s", folder);

    /*
     * We need to make sure there is message status space
     * for all the message numbers from 1 to "new" since
     * mhpath can select empty slots.  If we are adding
     * space at the end, we go ahead and add 10 slots.
     */
    if (mp->hghmsg >= mp->hghoff) {
	if (!(mp = folder_realloc (mp, 1, mp->hghmsg + 10)))
	    adios (NULL, "unable to allocate folder storage");
    } else if (mp->lowoff > 1) {
	if (!(mp = folder_realloc (mp, 1, mp->hghoff)))
	    adios (NULL, "unable to allocate folder storage");
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
