
/*
 * uprf.c -- "unsigned" lexical prefix
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


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
	c = (isalpha(c) && isupper(c)) ? tolower(c) : c;
	mask = (isalpha(mask) && isupper(mask)) ? tolower(mask) : mask;
	if (c != mask)
	    return 0;
	else
	    c1++;
    }
    return 1;
}
