
/*
 * folder_free.c -- free a folder/message structure
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


void
folder_free (struct msgs *mp)
{
    int i;

    if (!mp)
	return;

    if (mp->foldpath)
	free (mp->foldpath);

    /* free the sequence names */
    for (i = 0; mp->msgattrs[i]; i++)
	free (mp->msgattrs[i]);

    /* Close/free the sequence file if it is open */

    if (mp->seqhandle)
    	lkfclosedata (mp->seqhandle, mp->seqname);

    if (mp->seqname)
    	free (mp->seqname);

    free (mp->msgstats);	/* free message status area   */
    free (mp);			/* free main folder structure */
}
