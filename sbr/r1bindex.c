/* r1bindex.c -- Given a string and a character, return a pointer
 *            -- to the right of the rightmost occurrence of the
 *            -- character.  If the character doesn't occur, the
 *            -- pointer will be at the beginning of the string.
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "r1bindex.h"

/* Does not return NULL. */
char *
r1bindex(char *str, int chr)
{
    char *r;

    if (!chr)
        return str; /* Match old behaviour, don't know if it's used. */

    r = strrchr(str, chr);
    if (r)
        return r + 1;

    return str;
}
