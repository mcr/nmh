
/*
 * r1bindex.c -- Given a string and a character, return a pointer
 *            -- to the right of the rightmost occurrence of the
 *            -- character.  If the character doesn't occur, the
 *            -- pointer will be at the beginning of the string.
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


char *
r1bindex(char *str, int chr)
{
    char *cp;

    /* find null at the end of the string */
    for (cp = str; *cp; cp++)
	continue;

    /* backup to the rightmost character */
    --cp;

    /* now search for the rightmost occurrence of the character */
    while (cp >= str && *cp != chr)
	--cp;

    /* now move one to the right */
    return (++cp);
}
