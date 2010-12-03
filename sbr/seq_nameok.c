
/*
 * seq_nameok.c -- check if a sequence name is ok
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


int
seq_nameok (unsigned char *s)
{
    unsigned char *pp;

    if (s == NULL || *s == '\0') {
	advise (NULL, "empty sequence name");
	return 0;
    }

    /*
     * Make sure sequence name doesn't clash with one
     * of the `reserved' sequence names.
     */
    if (!(strcmp (s, "new") &&
	  strcmp (s, "all") &&
	  strcmp (s, "first") &&
	  strcmp (s, "last") &&
	  strcmp (s, "prev") &&
	  strcmp (s, "next"))) {
	advise (NULL, "illegal sequence name: %s", s);
	return 0;
    }

    /*
     * First character in a sequence name must be
     * an alphabetic character ...
     */
    if (!isalpha (*s)) {
	advise (NULL, "illegal sequence name: %s", s);
	return 0;
    }

    /*
     * and can be followed by zero or more alphanumeric characters
     */
    for (pp = s + 1; *pp; pp++)
	if (!isalnum (*pp)) {
	    advise (NULL, "illegal sequence name: %s", s);
	    return 0;
	}

    return 1;
}
