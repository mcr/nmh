
/*
 * folder_delmsgs.c -- "remove" SELECTED messages from a folder
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

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
folder_delmsgs (struct msgs *mp, int unlink_msgs)
{
    pid_t pid;
    int msgnum, vecp, retval = 0;
    char buf[100], *dp, **vec;

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

	if (mp->numsel > MAXARGS - 2)
	    adios (NULL, "more than %d messages for %s exec", MAXARGS - 2,
		   rmmproc);
	vec = (char **) calloc ((size_t) (mp->numsel + 2), sizeof(*vec));
	if (vec == NULL)
	    adios (NULL, "unable to allocate exec vector");
	vecp = 1;
	for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	    if (is_selected (mp, msgnum) &&
		!(vec[vecp++] = strdup (m_name (msgnum))))
		adios (NULL, "strdup failed");
	}
	vec[vecp] = NULL;

	fflush (stdout);
	vec[0] = r1bindex (rmmproc, '/');

	switch (pid = vfork()) {
	case -1:
	    advise ("fork", "unable to");
	    return -1;

	case 0:
	    execvp (rmmproc, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (rmmproc);
	    _exit (-1);

	default:
	    return (pidwait (pid, -1));
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

	    dp = m_name (msgnum);

	    if (unlink_msgs) {
		/* just unlink the messages */
		if (unlink (dp) == -1) {
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
	adios (NULL, "oops, mp->numsel should be 0");

    /* Mark that the sequence information has changed */
    mp->msgflags |= SEQMOD;

    return retval;
}
