
/*
 * seq_del.c -- delete message(s) from a sequence
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


/*
 * Delete all SELECTED messages from sequence
 *
 * If public ==  1, make sequence public.
 * If public ==  0, make sequence private.
 * If public == -1, leave the public/private bit alone for existing
 *                  sequences.  For new sequences, set this bit based
 *                  on its readonly status.
 *
 * If error, return 0, else return 1.
 */

int
seq_delsel (struct msgs *mp, char *cp, int public, int zero)
{
    unsigned int i;
    int msgnum, new_seq = 1;

    if (!seq_nameok (cp))
	return 0;

    /*
     * Get the number for this sequence
     */
    for (i = 0; i < svector_size (mp->msgattrs); i++) {
	if (!strcmp (svector_at (mp->msgattrs, i), cp)) {
	    new_seq = 0;
	    break;
	}
    }

    /*
     * If the zero flag is set, first add all existing
     * messages in this folder to the sequence.
     */
    if (zero) {
	/*
	 * create the sequence, if necessary
	 */
	if (new_seq) {
	    if (!(svector_push_back (mp->msgattrs, strdup (cp)))) {
		inform("strdup failed");
		return 0;
	    }
	}
	/*
	 * now add sequence bit to all existing messages
	 */
	for (msgnum = mp->lowmsg; msgnum <= mp->hghmsg; msgnum++) {
	    if (does_exist (mp, msgnum))
		add_sequence (mp, i, msgnum);
	    else
		clear_sequence (mp, i, msgnum);
	}
    } else {
	if (new_seq) {
	    inform("no such sequence as %s", cp);
	    return 0;
	}
    }

    /*
     * Now clear the bit on all selected messages
     */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	if (is_selected (mp, msgnum))
	    clear_sequence (mp, i, msgnum);

    if (! strcmp (cp, current)  &&
        mp->lowsel <= mp->curmsg  &&  mp->curmsg <= mp->hghsel) {
        /* Removed current message indication, so reset curmsg. */
        mp->curmsg = 0;
    }

    /*
     * Set the public/private bit for this sequence.
     */
    if (public == 1)
	make_seq_public (mp, i);
    else if (public == 0)
	make_seq_private (mp, i);
    else if (new_seq) {
	/*
	 * If public == -1, then only set the
	 * public/private bit for new sequences.
	 */
	if (is_readonly (mp))
	    make_seq_private (mp, i);
	else
	    make_seq_public (mp, i);
    }

    mp->msgflags |= SEQMOD;
    return 1;
}


/*
 * Delete message from sequence.
 *
 * If error, return 0, else return 1.
 */

int
seq_delmsg (struct msgs *mp, char *cp, int msgnum)
{
    size_t i;

    if (!seq_nameok (cp))
	return 0;

    for (i = 0; i < svector_size (mp->msgattrs); i++) {
	if (!strcmp (svector_at (mp->msgattrs, i), cp)) {
	    clear_sequence (mp, i, msgnum);
	    mp->msgflags |= SEQMOD;
	    return 1;
	}
    }

    inform("no such sequence as %s", cp);
    return 0;
}
