
/*
 * ssequal.c -- check if a string is a substring of another
 *
 * $Id$
 */

#include <h/mh.h>

/*
 * Check if s1 is a substring of s2.
 * If yes, then return 1, else return 0.
 */

int
ssequal (char *s1, char *s2)
{
    if (!s1)
	s1 = "";
    if (!s2)
	s2 = "";

    while (*s1)
	if (*s1++ != *s2++)
	    return 0;
    return 1;
}
