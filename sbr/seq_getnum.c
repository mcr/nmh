
/*
 * seq_getnum.c -- find the index for a sequence
 *              -- return -1 if sequence doesn't exist
 *
 * $Id$
 */

#include <h/mh.h>


int
seq_getnum (struct msgs *mp, char *seqname)
{
    int i;

    for (i = 0; mp->msgattrs[i]; i++)
	if (!strcmp (mp->msgattrs[i], seqname))
	    return i;

    return -1;
}
