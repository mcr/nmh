
/*
 * add.c -- If "s1" is NULL, this routine just creates a
 *       -- copy of "s2" into newly malloc'ed memory.
 *       --
 *       -- If "s1" is not NULL, then copy the concatenation
 *       -- of "s1" and "s2" (note the order) into newly
 *       -- malloc'ed memory.  Then free "s1".
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

char *
add (char *s2, char *s1)
{
    char *cp;
    size_t len1 = 0, len2 = 0;

    if (s1)
	len1 = strlen (s1);
    if (s2)
	len2 = strlen (s2);


    if (!(cp = malloc (len1 + len2 + 1)))
	adios (NULL, "unable to allocate string storage");

    /* Copy s1 and free it */
    if (s1) {
	memcpy (cp, s1, len1);
	free (s1);
    }

    /* Copy s2 */
    if (s2)
	memcpy (cp + len1, s2, len2);

    /* Now NULL terminate the string */
    cp[len1 + len2] = '\0';

    return cp;
}
