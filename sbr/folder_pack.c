
/*
 * folder_pack.c -- pack (renumber) the messages in a folder
 *               -- into a contiguous range from 1 to n.
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

/*
 * Pack the message in a folder.
 * Return -1 if error, else return 0.
 */

int
folder_pack (struct msgs **mpp, int verbose)
{
    int hole, msgnum, newcurrent = 0;
    char newmsg[BUFSIZ], oldmsg[BUFSIZ];
    struct msgs *mp;

    mp = *mpp;

    /*
     * Just return if folder is empty.
     */
    if (mp->nummsg == 0)
	return 0;

    /*
     * Make sure we have message status space allocated
     * for all numbers from 1 to current high message.
     */
    if (mp->lowoff > 1) {
	if ((mp = folder_realloc (mp, 1, mp->hghmsg)))
	    *mpp = mp;
	else {
	    advise (NULL, "unable to allocate folder storage");
	    return -1;
	}
    }

    for (msgnum = mp->lowmsg, hole = 1; msgnum <= mp->hghmsg; msgnum++) {
	if (does_exist (mp, msgnum)) {
	    if (msgnum != hole) {
		strncpy (newmsg, m_name (hole), sizeof(newmsg));
		strncpy (oldmsg, m_name (msgnum), sizeof(oldmsg));
		if (verbose)
		    printf ("message %s becomes %s\n", oldmsg, newmsg);

		/* move the message file */
		if (rename (oldmsg, newmsg) == -1) {
		    advise (newmsg, "unable to rename %s to", oldmsg);
		    return -1;
		}

		/* check if this is the current message */
		if (msgnum == mp->curmsg)
		    newcurrent = hole;

		/* copy the attribute flags for this message */
		copy_msg_flags (mp, hole, msgnum);

		if (msgnum == mp->lowsel)
		    mp->lowsel = hole;
		if (msgnum == mp->hghsel)
		    mp->hghsel = hole;

		/* mark that sequence information has been modified */
		mp->msgflags |= SEQMOD;
	    }
	    hole++;
	}
    }

    /* record the new number for the high/low message */
    mp->lowmsg = 1;
    mp->hghmsg = hole - 1;

    /* update the "cur" sequence */
    if (newcurrent != 0)
	seq_setcur (mp, newcurrent);

    return 0;
}
