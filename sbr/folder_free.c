/* folder_free.c -- free a folder/message structure
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "folder_free.h"
#include "h/utils.h"
#include "lock_file.h"


void
folder_free (struct msgs *mp)
{
    size_t i;
    struct bvector *v;

    if (!mp)
	return;

    free(mp->foldpath);

    /* free the sequence names */
    for (i = 0; i < svector_size (mp->msgattrs); i++)
	free (svector_at (mp->msgattrs, i));
    svector_free (mp->msgattrs);

    for (i = 0, v = mp->msgstats; i < mp->num_msgstats; ++i, ++v) {
	bvector_fini(v);
    }
    free (mp->msgstats);

    /* Close/free the sequence file if it is open */

    if (mp->seqhandle)
	lkfclosedata (mp->seqhandle, mp->seqname);

    free(mp->seqname);

    bvector_free (mp->attrstats);
    free (mp);			/* free main folder structure */
}
