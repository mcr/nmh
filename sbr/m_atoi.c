/* m_atoi.c -- Parse a string representation of a message number, and
 *          -- return the numeric value of the message.  If the string
 *          -- contains any non-digit characters, then return 0.
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "m_atoi.h"


int
m_atoi (char *str)
{
    int i;
    char *cp;

    for (i = 0, cp = str; *cp; cp++) {
	if (!isdigit((unsigned char) *cp))
	    return 0;

	i *= 10;
	i += (*cp - '0');
    }

    return i;
}
