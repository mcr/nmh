/* unquote.c -- Handle quote removal and quoted-pair strings on
 * RFC 2822-5322 atoms.
 *
 * This code is Copyright (c) 2013, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "unquote.h"

/*
 * Remove quotes (and handle escape strings) from RFC 5322 quoted-strings.
 *
 * Since we never add characters to the string, the output buffer is assumed
 * to have at least as many characters as the input string.
 *
 */

void
unquote_string(const char *input, char *output)
{
    int n = 0;	/* n is the position in the input buffer */
    int m = 0;	/* m is the position in the output buffer */

    while ( input[n] != '\0') {
	switch ( input[n] ) {
	case '\\':
	    n++;
	    if ( input[n] != '\0')
		output[m++] = input[n++];
	    break;
	case '"':
	    n++;
	    break;
	default:
	    output[m++] = input[n++];
	    break;
	}
    }

    output[m] = '\0';
}
