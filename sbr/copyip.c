/* copyip.c -- copy a string array and return pointer to end
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "copyip.h"


char **
copyip (char **p, char **q, int len_q)
{
    while (*p && --len_q > 0)
	*q++ = *p++;

    *q = NULL;

    return q;
}
