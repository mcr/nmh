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

#define is_fws(c) (c == '\t' || c == ' ' || c == '\n')

#define qpspecial(c) (c < ' ' || c == '=' || c == '?' || c == '_')

#define ENCODELINELIMIT	76

static void unfold_header(char **, int);
static int field_encode_address(const char *, char **, int, const char *);
static int field_encode_quoted(const char *, char **, const char *, int, int);
static int utf8len(const char *);

/*
 * Encode a message header using RFC 2047 encoding.  We make the assumption
 * that all characters < 128 are ASCII and as a consequence don't need any
 * encoding.
 */

int
encode_rfc2047(const char *name, char **value, int encoding,
	       const char *charset)
{
    int i, asciicount = 0, eightbitcount = 0, qpspecialcount = 0;
    char *p;

    /*
     * First, check to see if we even need to encode the header
     */

    for (p = *value; *p != '\0'; p++) {
	if (isascii((int) *p)) {
	    asciicount++;
	    if (qpspecial((int) *p))
	    	qpspecialcount++;
	} else
	    eightbitcount++;
    }

    if (eightbitcount == 0)
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
     * - Otherwise, we use quoted-printable.
     */

    if (encoding == CE_UNKNOWN)
    	encoding = (eightbitcount * 10 / (asciicount + eightbitcount) > 5) ?
						CE_BASE64 : CE_QUOTED;

    unfold_header(value, asciicount + eightbitcount);

    switch (encoding) {

#if 0
    case CE_BASE64:
    	return field_encode_base64(name, value, encoding, charset);
#endif

    case CE_QUOTED:
	return field_encode_quoted(name, value, charset, asciicount,
				   eightbitcount + qpspecialcount);

    default:
    	advise(NULL, "Internal error: unknown RFC-2047 encoding type");
	return 1;
    }
}

/*
 * Encode our specified header using quoted-printable
 */

static int
field_encode_quoted(const char *name, char **value, const char *charset,
		    int ascii, int encoded)
{
    int prefixlen = name ? strlen(name) + 2: 0, outlen = 0, column, newline = 1;
    int charsetlen = strlen(charset), utf8;
    char *output = NULL, *p, *q;

    /*
     * Right now we just encode the whole thing.  Maybe later on we'll
     * only encode things on a per-atom basis.
     */

    p = *value;

    column = prefixlen + 2;	/* Header name plus ": " */

    utf8 = strcasecmp(charset, "UTF-8") == 0;

    while (*p != '\0') {
    	/*
	 * Start a new line, if it's time
	 */
    	if (newline) {
	    /*
	     * If it's the start of the header, we don't need to pad it
	     *
	     * The length of the output string is ...
	     * =?charset?Q?...?=  so that's 7+strlen(charset) + 2 for \n NUL
	     *
	     * plus 1 for every ASCII character and 3 for every eight bit
	     * or special character (eight bit characters are written as =XX).
	     *
	     */

	    int tokenlen;

	    outlen += 9 + charsetlen + ascii + 3 * encoded;

	    /*
	     * If output is set, then we're continuing the header.  Otherwise
	     * do the initial allocation.
	     */

	    if (output) {
	        int curlen = q - output, i;
		outlen += prefixlen + 1;	/* Header plus \n ": " */
		output = mh_xrealloc(output, outlen);
		q = output + curlen;
		*q++ = '?';
		*q++ = '=';
		*q++ = '\n';
		for (i = 0; i < prefixlen; i++)
		    *q++ = ' ';
	    } else {
	    	/*
		 * A bit of a hack here; the header can contain multiple
		 * spaces (probably at least one) until we get to the
		 * actual text.  Copy until we get to a non-space.
		 */
	    	output = mh_xmalloc(outlen);
		q = output;
		while (is_fws(*p))
		    *q++ = *p++;
	    }

	    tokenlen = snprintf(q, outlen - (q - output), "=?%s?Q?", charset);
	    q += tokenlen;
	    column = prefixlen + tokenlen;
	    newline = 0;
	}

	/*
	 * Process each character, encoding if necessary
	 */

	column++;

	if (*p == ' ') {
	    *q++ = '_';
	    ascii--;
	} else if (isascii((int) *p) && !qpspecial((int) *p)) {
	    *q++ = *p;
	    ascii--;
	} else {
	    snprintf(q, outlen - (q - output), "=%02X", *((unsigned char *) p));
	    q += 3;
	    column += 2;	/* column already incremented by 1 above */
	    encoded--;
	}

	p++;

	/*
	 * We're not allowed more than ENCODELINELIMIT characters per line,
	 * so reserve some room for the final ?=.
	 *
	 * If prefixlen == 0, we haven't been passed in a header name, so
	 * don't ever wrap the field (we're likely doing an address).
	 */

	if (prefixlen == 0)
	    continue;

	if (column >= ENCODELINELIMIT - 2) {
	    newline = 1;
	} else if (utf8) {
	    /*
	     * Okay, this is a bit weird, but to explain a bit more ...
	     *
	     * RFC 2047 prohibits the splitting of multibyte characters
	     * across encoded words.  Right now we only handle the case
	     * of UTF-8, the most common multibyte encoding.
	     *
	     * p is now pointing at the next input character.  If we're
	     * using UTF-8 _and_ we'd go over ENCODELINELIMIT given the
	     * length of the complete character, then trigger a newline
	     * now.  Note that we check the length * 3 since we have to
	     * allow for the encoded output.
	     */
	    if (column + (utf8len(p) * 3) > ENCODELINELIMIT - 2) {
	    	newline = 1;
	    }
	}
    }

    strcat(q, "?=\n");

    free(*value);

    *value = output;

    return 0;
}

/*
 * Calculate the length of a UTF-8 character.
 *
 * If it's not a UTF-8 character (or we're in the middle of a multibyte
 * character) then simply return 0.
 */

static int
utf8len(const char *p)
{
    int len = 1;

    if (*p == '\0')
    	return 0;

    if (isascii((int) *p) || (*((unsigned char *) p) & 0xc0) == 0x80)
    	return 0;

    p++;
    while ((*((unsigned char *) p++) & 0xc0) == 0x80)
    	len++;

    return len;
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
	     *
	     * This has the side effect of stripping off the final newline
	     * for the header; we put it back in the encoding routine.
	     */
	    while (is_fws(*q++))
	    	;
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

static int
field_encode_address(const char *name, char **value, int encoding,
		     const char *charset)
{
    return 0;
}
