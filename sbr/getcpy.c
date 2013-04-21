
/*
 * getcpy.c -- copy a string in managed memory
 *
 * THIS IS OBSOLETE.  NEED TO REPLACE ALL OCCURENCES
 * OF GETCPY WITH STRDUP.  BUT THIS WILL REQUIRE
 * CHANGING PARTS OF THE CODE TO DEAL WITH NULL VALUES.
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>


char *
getcpy (const char *str)
{
    char *cp;
    size_t len;

    if (str) {
	len = strlen(str) + 1;
	cp = mh_xmalloc (len);
	memcpy (cp, str, len);
    } else {
	cp = mh_xmalloc ((size_t) 1);
	*cp = '\0';
    }
    return cp;
}
