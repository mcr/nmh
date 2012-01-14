
/*
 * seq_add.c -- add message(s) to a sequence
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


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
    int msgnum, new_seq = 1;

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
    for (i = 0; mp->msgattrs[i]; i++) {
	if (!strcmp (mp->msgattrs[i], cp)) {
	    new_seq = 0;
	    break;
	}
    }

    /*
     * If this is a new sequence, add a slot for it
     */
    if (new_seq) {
	if (i >= NUMATTRS) {
	    advise (NULL, "only %d sequences allowed (no room for %s)!", NUMATTRS, cp);
	    return 0;
	}
	if (!(mp->msgattrs[i] = strdup (cp))) {
	    advise (NULL, "strdup failed");
	    return 0;
	}
	mp->msgattrs[i + 1] = NULL;
    }

    /*
     * If sequence is new, or zero flag is set, then first
     * clear the bit for this sequence from all messages.
     */
    if (new_seq || zero) {
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
    int j, new_seq = 1;

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
    for (i = 0; mp->msgattrs[i]; i++) {
	if (!strcmp (mp->msgattrs[i], cp)) {
	    new_seq = 0;
	    break;
	}
    }

    /*
     * If this is a new sequence, add a slot for it
     */
    if (new_seq) {
	if (i >= NUMATTRS) {
	    advise (NULL, "only %d sequences allowed (no room for %s)!", NUMATTRS, cp);
	    return 0;
	}
	if (!(mp->msgattrs[i] = strdup (cp))) {
	    advise (NULL, "strdup failed");
	    return 0;
	}
	mp->msgattrs[i + 1] = NULL;
    }

    /*
     * If sequence is new, or zero flag is set, then first
     * clear the bit for this sequence from all messages.
     */
    if (new_seq || zero) {
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
