
/*
 * strdup.c -- duplicate a string
 *
 * $Id$
 */

#include <h/mh.h>


char *
strdup (char *str)
{
    char *cp;
    size_t len;

    if (!str)
	return NULL;

    len = strlen(str) + 1;
    if (!(cp = malloc (len)))
	return NULL;
    memcpy (cp, str, len);
    return cp;
}
