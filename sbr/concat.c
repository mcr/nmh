
/*
 * concat.c -- concatenate a variable number (minimum of 1)
 *             of strings in managed memory
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>


char *
concat (const char *s1, ...)
{
    char *cp, *dp, *sp;
    size_t len;
    va_list list;

    len = strlen (s1) + 1;
    va_start(list, s1); 
    while ((cp = va_arg(list, char *)))
	len += strlen (cp);
    va_end(list);

    dp = sp = mh_xmalloc(len);

    sp = copy(s1, sp);

    va_start(list, s1); 
    while ((cp = va_arg (list, char *)))
	sp = copy(cp, sp);
    va_end(list);

    return dp;
}
