
/*
 * copyip.c -- copy a string array and return pointer to end
 *
 * $Id$
 */

#include <h/mh.h>


char **
copyip (char **p, char **q, int len_q)
{
    while (*p && --len_q > 0)
	*q++ = *p++;

    *q = NULL;

    return q;
}
