
/*
 * m_atoi.c -- Parse a string representation of a message number, and
 *          -- return the numeric value of the message.  If the string
 *          -- contains any non-digit characters, then return 0.
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


int
m_atoi (char *str)
{
    int i;
    char *cp;

    for (i = 0, cp = str; *cp; cp++) {
#ifdef LOCALE
	if (!isdigit(*cp))
#else
	if (*cp < '0' || *cp > '9')
#endif
	    return 0;

	i *= 10;
	i += (*cp - '0');
    }

    return i;
}
