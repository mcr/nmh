
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
    size_t i;

    for (i = 0; i < svector_size (mp->msgattrs); i++)
	if (!strcmp (svector_at (mp->msgattrs, i), seqname))
	    return i;

    return -1;
}
