
/*
 * concat.c -- concatenate a variable number (minimum of 1)
 *             of strings in managed memory
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


char *
concat (char *s1, ...)
{
    char *cp, *dp, *sp;
    size_t len;
    va_list list;

    len = strlen (s1) + 1;
    va_start(list, s1); 
    while ((cp = va_arg(list, char *)))
	len += strlen (cp);
    va_end(list);

    if (!(dp = sp = malloc(len)))
	adios (NULL, "unable to allocate string storage");

    sp = copy(s1, sp);

    va_start(list, s1); 
    while ((cp = va_arg (list, char *)))
	sp = copy(cp, sp);
    va_end(list);

    return dp;
}
