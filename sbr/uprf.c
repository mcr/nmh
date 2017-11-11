/* uprf.c -- "unsigned" lexical prefix
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "uprf.h"


/* uprf returns true if s starts with prefix, ignoring case.
 * Otherwise false.  If s or prefix are NULL then false results. */
int
uprf(const char *s, const char *prefix)
{
    unsigned char *us, *up;

    if (!s || !prefix)
	return 0;
    us = (unsigned char *)s;
    up = (unsigned char *)prefix;

    while (*us && tolower(*us) == tolower(*up)) {
        us++;
        up++;
    }

    return *up == '\0';
}
