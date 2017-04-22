/* folder_realloc.c -- realloc a folder/msgs structure
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>

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
	bvector_t *v;
	size_t old_size = mp->num_msgstats;
	size_t new_size = hi - lo + 1;
	size_t i;

	/*
	 * We are just extending (or shrinking) the end of message
	 * status array.  So we don't have to move anything and can
	 * just realloc the message status array.
	 */

	for (i = old_size, v = &mp->msgstats[old_size-1]; i > new_size;
	     --i, --v) {
	    bvector_free (*v);
	}
	mp->num_msgstats = MSGSTATNUM (lo, hi);
	mp->msgstats =
	    mh_xrealloc (mp->msgstats, MSGSTATSIZE(mp));
	for (i = old_size, v = &mp->msgstats[old_size]; i < new_size;
	     ++i, ++v) {
	    *v = bvector_create (0);
	}
    } else {
	/*
	 * We are changing the offset of the message status
	 * array.  So we will need to shift everything.
	 */
	bvector_t *tmpstats, *t;
	size_t i;

	/* first allocate the new message status space */
	mp->num_msgstats = MSGSTATNUM (lo, hi);
	tmpstats = mh_xmalloc (MSGSTATSIZE(mp));
	for (i = 0, t = tmpstats; i < mp->num_msgstats; ++i, ++t) {
	    *t = bvector_create (0);
	}

	/* then copy messages status array with shift */
	if (mp->nummsg > 0) {
	    for (msgnum = mp->lowmsg; msgnum <= mp->hghmsg; msgnum++)
		bvector_copy (tmpstats[msgnum - lo], msgstat (mp, msgnum));
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
