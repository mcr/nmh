/* rmm.c -- remove a message(s)
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/error.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/m_maildir.h"

#define RMM_SWITCHES \
    X("unlink", 0, UNLINKSW) \
    X("nounlink", 0, NUNLINKSW) \
    X("rmmproc program", 0, RPROCSW) \
    X("normmproc", 0, NRPRCSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(RMM);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(RMM, switches);
#undef X


int
main (int argc, char **argv)
{
    int msgnum;
    bool unlink_msgs = false;
    char *cp, *maildir, *folder = NULL;
    char buf[BUFSIZ], **argp;
    char **arguments;
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp;

    if (nmh_init(argv[0], true, true)) { return 1; }

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
		die("-%s unknown\n", cp);

	    case HELPSW:
		snprintf (buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
			  invo_name);
		print_help (buf, switches, 1);
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case UNLINKSW:
		unlink_msgs = true;
		continue;
	    case NUNLINKSW:
		unlink_msgs = false;
		continue;

            case RPROCSW:
                if (!(rmmproc = *argp++) || *rmmproc == '-')
                    die("missing argument to %s", argp[-2]);
                continue;
            case NRPRCSW:
                rmmproc = NULL;
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
    seq_setprev (mp);		/* set the previous-sequence      */

    /*
     * As part of the new world locking order, folder_delmsgs() now updates
     * the sequence and context for us.  But since folder_delmsgs() doesn't
     * have access to the folder name, change the context now.
     */

    context_replace (pfolder, folder);

    /* "remove" the SELECTED messages */
    folder_delmsgs (mp, unlink_msgs, 0);

    folder_free (mp);			/* free folder structure   */
    done (0);
    return 1;
}
