/* seq_add.c -- add message(s) to a sequence
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "seq_add.h"
#include "error.h"


/*
 * Add all the SELECTED messages to a (possibly new) sequence.
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
seq_addsel (struct msgs *mp, char *cp, int public, int zero)
{
    unsigned int i;
    int msgnum;

    if (!seq_nameok (cp))
	return 0;

    /*
     * We keep mp->curmsg and "cur" sequence in sync.
     * See seq_list() and seq_init().
     */
    if (!strcmp (current,cp))
	mp->curmsg = mp->hghsel;

    /*
     * Get the number for this sequence
     */
    bool new_seq = true;
    for (i = 0; i < svector_size (mp->msgattrs); i++) {
	if (!strcmp (svector_at (mp->msgattrs, i), cp)) {
	    new_seq = false;
	    break;
	}
    }

    /*
     * If this is a new sequence, add a slot for it
     */
    if (new_seq) {
	if (!(svector_push_back (mp->msgattrs, strdup (cp)))) {
	    inform("strdup failed");
	    return 0;
	}
    }

    /*
     * If sequence is new, or zero flag is set, then first
     * clear the bit for this sequence from all messages.
     */
    if ((new_seq || zero) && mp->nummsg > 0) {
	for (msgnum = mp->lowmsg; msgnum <= mp->hghmsg; msgnum++)
	    clear_sequence (mp, i, msgnum);
    }

    /*
     * Now flip on the bit for this sequence
     * for all selected messages.
     */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	if (is_selected (mp, msgnum))
	    add_sequence (mp, i, msgnum);

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
 * Add a message to a (possibly new) sequence.
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
seq_addmsg (struct msgs *mp, char *cp, int msgnum, int public, int zero)
{
    unsigned int i;
    int j;

    if (!seq_nameok (cp))
	return 0;

    /*
     * keep mp->curmsg and msgattrs["cur"] in sync - see seq_list()
     */
    if (!strcmp (current,cp))
	mp->curmsg = msgnum;	

    /*
     * Get the number for this sequence
     */
    bool new_seq = true;
    for (i = 0; i < svector_size (mp->msgattrs); i++) {
	if (!strcmp (svector_at (mp->msgattrs, i), cp)) {
	    new_seq = false;
	    break;
	}
    }

    /*
     * If this is a new sequence, add a slot for it
     */
    if (new_seq) {
	if (!(svector_push_back (mp->msgattrs, strdup (cp)))) {
	    inform("strdup failed");
	    return 0;
	}
    }

    /*
     * If sequence is new, or zero flag is set, then first
     * clear the bit for this sequence from all messages.
     */
    if ((new_seq || zero) && mp->nummsg > 0) {
	for (j = mp->lowmsg; j <= mp->hghmsg; j++)
	    clear_sequence (mp, i, j);
    }

    /*
     * Now flip on the bit for this sequence
     * for this particular message.
     */
    add_sequence (mp, i, msgnum);

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
