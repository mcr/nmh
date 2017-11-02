/* strindex.c -- "unsigned" lexical index
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"

int
stringdex (char *p1, char *p2)
{
    char *p;

    if (p1 == NULL || p2 == NULL)
	return -1;

    for (p = p2; *p; p++)
	if (uprf (p, p1))
	    return p - p2;

    return -1;
}
