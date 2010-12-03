
/*
 * seq_getnum.c -- find the index for a sequence
 *              -- return -1 if sequence doesn't exist
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
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
