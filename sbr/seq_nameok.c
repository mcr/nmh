/* seq_nameok.c -- check if a sequence name is ok
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "seq_nameok.h"
#include "error.h"


int
seq_nameok (char *s)
{
    char *pp;

    if (s == NULL || *s == '\0') {
	inform("empty sequence name");
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
        inform("sequence name is reserved: %s", s);
	return 0;
    }

    /*
     * First character in a sequence name must be
     * an alphabetic character ...
     */
    if (!isalpha ((unsigned char) *s)) {
        inform("sequence name must start with letter: %s", s);
	return 0;
    }

    /*
     * and can be followed by zero or more alphanumeric characters
     */
    for (pp = s + 1; *pp; pp++)
	if (!isalnum ((unsigned char) *pp)) {
            inform("sequence name must be alphanumeric: %s", s);
	    return 0;
	}

    return 1;
}
