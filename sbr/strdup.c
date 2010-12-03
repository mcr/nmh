
/*
 * strdup.c -- duplicate a string
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>


char *
strdup (const char *str)
{
    char *cp;
    size_t len;

    if (!str)
	return NULL;

    len = strlen(str) + 1;
    cp = mh_xmalloc (len);
    memcpy (cp, str, len);
    return cp;
}
