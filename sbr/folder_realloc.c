
/*
 * folder_realloc.c -- realloc a folder/msgs structure
 *
 * $Id$
 */

#include <h/mh.h>

/*
 * Reallocate some of the space in the folder
 * structure (currently just message status array).
 *
 * Return pointer to new folder structure.
 * If error, return NULL.
 */

struct msgs *
folder_realloc (struct msgs *mp, int lo, int hi)
{
    int msgnum;

    /* sanity checks */
    if (lo < 1)
	adios (NULL, "BUG: called folder_realloc with lo (%d) < 1", lo);
    if (hi < 1)
	adios (NULL, "BUG: called folder_realloc with hi (%d) < 1", hi);
    if (mp->nummsg > 0 && lo > mp->lowmsg)
	adios (NULL, "BUG: called folder_realloc with lo (%d) > mp->lowmsg (%d)",
	       lo, mp->lowmsg);
    if (mp->nummsg > 0 && hi < mp->hghmsg)
	adios (NULL, "BUG: called folder_realloc with hi (%d) < mp->hghmsg (%d)",
	       hi, mp->hghmsg);

    /* Check if we really need to reallocate anything */
    if (lo == mp->lowoff && hi == mp->hghoff)
	return mp;

    if (lo == mp->lowoff) {
	/*
	 * We are just extending (or shrinking) the end of message
	 * status array.  So we don't have to move anything and can
	 * just realloc the message status array.
	 */
	if (!(mp->msgstats = realloc (mp->msgstats, MSGSTATSIZE(mp, lo, hi)))) {
	    advise (NULL, "unable to reallocate message storage");
	    return NULL;
	}
    } else {
	/*
	 * We are changing the offset of the message status
	 * array.  So we will need to shift everything.
	 */
	seqset_t *tmpstats;

	/* first allocate the new message status space */
	if (!(tmpstats = malloc (MSGSTATSIZE(mp, lo, hi)))) {
	    advise (NULL, "unable to reallocate message storage");
	    return NULL;
	}

	/* then copy messages status array with shift */
	if (mp->nummsg > 0) {
	    for (msgnum = mp->lowmsg; msgnum <= mp->hghmsg; msgnum++)
		tmpstats[msgnum - lo] = mp->msgstats[msgnum - mp->lowoff];
	}
	free(mp->msgstats);
	mp->msgstats = tmpstats;
    }

    mp->lowoff = lo;
    mp->hghoff = hi;

    /*
     * Clear all the flags for entries outside
     * the current message range for this folder.
     */
    if (mp->nummsg > 0) {
	for (msgnum = mp->lowoff; msgnum < mp->lowmsg; msgnum++)
	    clear_msg_flags (mp, msgnum);
	for (msgnum = mp->hghmsg + 1; msgnum <= mp->hghoff; msgnum++)
	    clear_msg_flags (mp, msgnum);
    } else {
	/* no messages, so clear entire range */
	for (msgnum = mp->lowoff; msgnum <= mp->hghoff; msgnum++)
	    clear_msg_flags (mp, msgnum);
    }

    return mp;
}
