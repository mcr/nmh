/*
 * Routines to encode message headers using RFC 2047-encoding.
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/mhparse.h>
#include <h/utils.h>

/*
 * List of headers that contain addresses and as a result require special
 * handling
 */

static char *address_headers[] = {
    "To",
    "From",
    "cc",
    "Bcc",
    "Reply-To",
    "Sender",
    "Resent-To",
    "Resent-From",
    "Resent-cc",
    "Resent-Bcc",
    "Resent-Reply-To",
    "Resent-Sender",
    NULL,
};

/*
 * Macros we use for parsing headers
 */

#define is_fws(c) (c == '\t' || c == ' ')

static void unfold_header(char **, int);
static int field_encode_address(const char *, char **, int, const char *);

/*
 * Encode a message header using RFC 2047 encoding.  We make the assumption
 * that all characters < 128 are ASCII and as a consequence don't need any
 * encoding.
 */

int
encode_rfc2047(const char *name, char **value, int encoding,
	       const char *charset)
{
    int i, count = 0, len;
    char *p;

    /*
     * First, check to see if we even need to encode the header
     */

    for (p = *value; *p != '\0'; p++) {
	if (! isascii((int) *p))
	count++;
    }

    if (count == 0)
    	return 0;

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

    /*
     * If charset was NULL, then get the value from the locale.  But
     * we reject it if it returns US-ASCII
     */

    if (charset == NULL)
    	charset = write_charset_8bit();

    if (strcasecmp(charset, "US-ASCII") == 0) {
    	advise(NULL, "Cannot use US-ASCII with 8 bit characters in header");
	return 1;
    }

    /*
     * If we have an address header, then we need to parse the addresses
     * and only encode the names or comments.  Otherwise, handle it normally.
     */

    for (i = 0; address_headers[i]; i++) {
    	if (strcasecmp(name, address_headers[i]) == 0)
	    return field_encode_address(name, value, encoding, charset);
    }

    /*
     * On the encoding we choose, and the specifics of encoding:
     *
     * - If a specified encoding is passed in, we use that.
     * - If more than 50% of the characters are high-bit, we use base64
     *   and encode the whole field as one atom (possibly split).
     *   Otherwise, we use quoted-printable.
     * - If more than 10% of the characters are high-bit, then we encode
     *   the entire header as one (possibly split) atom.  Otherwise,
     *   take each atom as they come and encode it on a per-atom basis.
     */

    len = strlen(*value);

    if (encoding == CE_UNKNOWN)
    	encoding = (count * 10 / len > 5) ? CE_BASE64 : CE_QUOTED;

    switch (encoding) {

    case CE_BASE64:
    	return field_encode_base64(value, charset, len, NULL);

    case CE_QUOTED:
    	if (count * 100 / len > 10) {
	    return field_encode_quoted(value, charset, len, NULL);
	} else {
	    /*
	     * Break it down by atoms.
	     */

	    unfold_header(value, len);
	}
    default:
    	advise(NULL, "Internal error: unknown RFC-2047 encoding type");
	return 1;
    }

    return 0;
}

/*
 * "Unfold" a header, making it a single line (without continuation)
 *
 * We cheat a bit here; we never make the string longer, so using the
 * original length here is fine.
 */

static void
unfold_header(char **value, int len)
{
    char *str = mh_xmalloc(len + 1);
    char *p = str, *q = *value;

    while (*q != '\0') {
    	if (*q == '\n') {
	    /*
	     * When we get a newline, skip to the next non-whitespace
	     * character and add a space to replace all of the whitespace
	     */
	    while (is_fws(*q))
	    	q++;
	    if (*q == '\0')
	    	break;

	    *p++ = ' ';
	} else {
	    *p++ = *q++;
	}
    }

    *p = '\0';

    free(*value);
    *value = str;
}
