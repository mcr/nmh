/* folder_delmsgs.c -- "remove" SELECTED messages from a folder
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "folder_delmsgs.h"
#include "context_save.h"
#include "arglist.h"
#include "error.h"
#include "h/utils.h"
#include "m_mktemp.h"

/*
 * 1) If we are using an external rmmproc, then exec it.
 * 2) Else if unlink_msgs is non-zero, then unlink the
 *    SELECTED messages.
 * 3) Else rename SELECTED messages by prefixing name
 *    with a standard prefix.
 *
 * If there is an error, return -1, else return 0.
 */

int
folder_delmsgs (struct msgs *mp, int unlink_msgs, int nohook)
{
    pid_t pid;
    int msgnum, vecp, retval = 0;
    char buf[100], *dp, **vec, *prog;
    char	msgpath[BUFSIZ];

    /*
     * If "rmmproc" is defined, exec it to remove messages.
     */
    if (rmmproc) {
	/* Unset the EXISTS flag for each message to be removed */
	for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	    if (is_selected (mp, msgnum))
		unset_exists (mp, msgnum);
	}

	/* Mark that the sequence information has changed */
	mp->msgflags |= SEQMOD;

	/*
	 * Write out the sequence and context files; this will release
	 * any locks before the rmmproc is called.
	 */

	seq_save (mp);
	context_save ();

	vec = argsplit(rmmproc, &prog, &vecp);

	/*
	 * argsplit allocates a MAXARGS vector by default,  If we need
	 * something bigger, allocate it ourselves
	 */

	if (mp->numsel + vecp + 1 > MAXARGS)
	    vec = mh_xrealloc(vec, (mp->numsel + vecp + 1) * sizeof *vec);
	for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	    if (is_selected (mp, msgnum) &&
		!(vec[vecp++] = strdup (m_name (msgnum))))
		die("strdup failed");
	}
	vec[vecp] = NULL;

	fflush (stdout);

	switch (pid = fork()) {
	case -1:
	    advise ("fork", "unable to");
	    return -1;

	case 0:
	    execvp (prog, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (rmmproc);
	    _exit(1);

	default:
	    arglist_free(prog, vec);
	    return pidwait(pid, -1);
	}
    }

    /*
     * Either unlink or rename the SELECTED messages
     */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (is_selected (mp, msgnum)) {
	    /* unselect message */
	    unset_selected (mp, msgnum);
	    mp->numsel--;

	    /*
	     *	Run the external hook on the message if one was specified in the context.
	     *	All we have is the message number; we have changed to the directory
	     *	containing the message.  So, we need to extract that directory to form
	     *	the complete path.  Note that the caller knows the directory, but has
	     *	no way of passing that to us.
	     */

	    if (!nohook) {
		    (void)snprintf(msgpath, sizeof (msgpath), "%s/%d", mp->foldpath, msgnum);
		    (void)ext_hook("del-hook", msgpath, NULL);
		}

	    dp = m_name (msgnum);

	    if (unlink_msgs) {
		/* just unlink the messages */
		if (m_unlink (dp) == -1) {
		    admonish (dp, "unable to unlink");
		    retval = -1;
		    continue;
		}
	    } else {
		/* or rename messages with standard prefix */
		strncpy (buf, m_backup (dp), sizeof(buf));
		if (rename (dp, buf) == -1) {
		    admonish (buf, "unable to rename %s to", dp);
		    retval = -1;
		    continue;
		}
	    }

	    /* If removal was successful, decrement message count */
	    unset_exists (mp, msgnum);
	    mp->nummsg--;
	}
    }

    /* Sanity check */
    if (mp->numsel != 0)
	die("oops, mp->numsel should be 0");

    /* Mark that the sequence information has changed */
    mp->msgflags |= SEQMOD;

    /*
     * Write out sequence and context files
     */

    seq_save (mp);
    context_save ();

    return retval;
}
