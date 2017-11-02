/* folder_realloc.c -- realloc a folder/msgs structure
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "error.h"
#include "h/utils.h"

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
    struct bvector *tmpstats, *t;
    size_t i;
    int msgnum;

    /* sanity checks */
    if (lo < 1)
	die("BUG: called folder_realloc with lo (%d) < 1", lo);
    if (hi < 1)
	die("BUG: called folder_realloc with hi (%d) < 1", hi);
    if (mp->nummsg > 0 && lo > mp->lowmsg)
	die("BUG: called folder_realloc with lo (%d) > mp->lowmsg (%d)",
	       lo, mp->lowmsg);
    if (mp->nummsg > 0 && hi < mp->hghmsg)
	die("BUG: called folder_realloc with hi (%d) < mp->hghmsg (%d)",
	       hi, mp->hghmsg);

    /* Check if we really need to reallocate anything */
    if (lo == mp->lowoff && hi == mp->hghoff)
	return mp;

    /* first allocate the new message status space */
    mp->num_msgstats = MSGSTATNUM (lo, hi);
    tmpstats = mh_xmalloc (MSGSTATSIZE(mp));
    for (i = 0, t = tmpstats; i < mp->num_msgstats; ++i, ++t) {
        bvector_init(t);
    }

    /* then copy messages status array with shift */
    if (mp->nummsg > 0) {
        for (msgnum = mp->lowmsg; msgnum <= mp->hghmsg; msgnum++)
            bvector_copy (tmpstats + msgnum - lo, msgstat (mp, msgnum));
    }
    free(mp->msgstats);
    mp->msgstats = tmpstats;
    mp->lowoff = lo;
    mp->hghoff = hi;

    return mp;
}
