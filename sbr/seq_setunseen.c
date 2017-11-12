/* seq_setunseen.c -- add/delete all messages which have the SELECT_UNSEEN
 *                 -- bit set to/from the Unseen-Sequence
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "seq_setunseen.h"
#include "context_find.h"
#include "brkstring.h"
#include "seq_getnum.h"
#include "seq_del.h"
#include "seq_add.h"
#include "h/utils.h"

/*
 * We scan through the folder and act upon all messages
 * that are marked with the SELECT_UNSEEN bit.
 *
 * If seen == 1, delete messages from unseen sequence.
 * If seen == 0, add messages to unseen sequence.
 */

void
seq_setunseen (struct msgs *mp, int seen)
{
    int msgnum;
    char **ap, *cp, *dp;

    /*
     * Get the list of sequences for Unseen-Sequence
     * and split them.
     */
    if (!(cp = context_find (usequence)))
        return;
    dp = mh_xstrdup(cp);
    if (!(ap = brkstring (dp, " ", "\n")) || !*ap) {
        free (dp);
        return;
    }

    /*
     * Now add/delete each message which has the SELECT_UNSEEN
     * bit set to/from each of these sequences.
     */
    for (; *ap; ap++) {
	if (seen) {
	    /* make sure sequence exists first */
	    if (seq_getnum(mp, *ap) != -1)
		for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
		    if (is_unseen (mp, msgnum))
			seq_delmsg (mp, *ap, msgnum);
	} else {
	    for (msgnum = mp->lowmsg; msgnum <= mp->hghmsg; msgnum++)
		if (is_unseen (mp, msgnum))
		    seq_addmsg (mp, *ap, msgnum, -1, 0);
	}
    }

    free (dp);
}
