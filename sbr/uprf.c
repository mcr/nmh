
/*
 * uprf.c -- "unsigned" lexical prefix
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


/* uprf returns true if c1 starts with c2, ignoring case.
 * Otherwise false.  If c1 or c2 are NULL then false results. */
int
uprf (const char *c1, const char *c2)
{
    int c, mask;

    if (!(c1 && c2))
	return 0;

    while ((c = *c2++))
    {
	c &= 0xff;
	mask = *c1 & 0xff;
	c = tolower(c);
	mask = tolower(mask);
	if (c != mask)
	    return 0;
        c1++;
    }
    return 1;
}
