
/*
 * atooi.c -- octal version of atoi()
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


int
atooi(char *cp)
{
    register int i, base;

    i = 0;
    base = 8;
    while (*cp >= '0' && *cp <= '7') {
	i *= base;
	i += *cp++ - '0';
    }

    return i;
}
