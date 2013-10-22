/*
 * Routines to encode message headers using RFC 2047-encoding.
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

/*
 * List of headers that contain addresses and as a result require special
 * handling
 */

/*
 * Encode a message header using RFC 2047 encoding.  We make the assumption
 * that all characters < 128 are ASCII and as a consequence don't need any
 * encoding.
 */

int
encode_rfc2047(const char *name, char **value, int encoding,
	       const char *charset)
{
    char *p;

    /*
     * First, check to see if we even need to encode the header
     */

    for (p = *value; *p != '\0'; p++) {
	if (! isascii((int) *p)
	    goto encode;
    }

    return 0;

encode:
    /*
     * Some rules from RFC 2047:
     *
     * - Encoded words cannot be more than 75 characters long
     * - Multiple "long" encoded words must be on new lines.
     *
     * Also, we're not permitted to encode email addresses, so
     * we need to actually _parse_ email addresses and only encode
     * the right bits.  
     */
