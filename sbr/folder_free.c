
/*
 * folder_free.c -- free a folder/message structure
 *
 * $Id$
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

    free (mp->msgstats);	/* free message status area   */
    free (mp);			/* free main folder structure */
}
