
/*
 * getcpy.c -- copy a string in managed memory
 *
 * THIS IS OBSOLETE.  NEED TO REPLACE ALL OCCURENCES
 * OF GETCPY WITH STRDUP.  BUT THIS WILL REQUIRE
 * CHANGING PARTS OF THE CODE TO DEAL WITH NULL VALUES.
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


char *
getcpy (char *str)
{
    char *cp;
    size_t len;

    if (str) {
	len = strlen(str) + 1;
	if (!(cp = malloc (len)))
	    adios (NULL, "unable to allocate string storage");
	memcpy (cp, str, len);
    } else {
	if (!(cp = malloc ((size_t) 1)))
	    adios (NULL, "unable to allocate string storage");
	*cp = '\0';
    }
    return cp;
}
