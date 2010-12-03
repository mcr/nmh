
/*
 * rmm.c -- remove a message(s)
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>

static struct swit switches[] = {
#define UNLINKSW      0
    { "unlink", 0 },
#define NUNLINKSW    1
    { "nounlink", 0 },
#define VERSIONSW     2
    { "version", 0 },
#define	HELPSW        3
    { "help", 0 },
    { NULL, 0 }
};


int
main (int argc, char **argv)
{
    int msgnum, unlink_msgs = 0;
    char *cp, *maildir, *folder = NULL;
    char buf[BUFSIZ], **argp;
    char **arguments;
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

    /* parse arguments */
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

	    case UNLINKSW:
		unlink_msgs++;
		continue;
	    case NUNLINKSW:
		unlink_msgs = 0;
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
    seq_setprev (mp);		/* set the previous-sequence      */

    /*
     * This is hackish.  If we are using a external rmmproc,
     * then we need to update the current folder in the
     * context so the external rmmproc will remove files
     * from the correct directory.  This should be moved to
     * folder_delmsgs().
     */
    if (rmmproc) {
	context_replace (pfolder, folder);
	context_save ();
	fflush (stdout);
    }

    /* "remove" the SELECTED messages */
    folder_delmsgs (mp, unlink_msgs, 0);

    seq_save (mp);		/* synchronize message sequences  */
    context_replace (pfolder, folder);	/* update current folder   */
    context_save ();			/* save the context file   */
    folder_free (mp);			/* free folder structure   */
    done (0);
    return 1;
}
