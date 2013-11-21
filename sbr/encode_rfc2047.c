/*
 * Routines to encode message headers using RFC 2047-encoding.
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/mhparse.h>
#include <h/addrsbr.h>
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

#define qphrasevalid(c) ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || \
			 (c >= 'a' && c <= 'z') || \
			 c == '!' || c == '*' || c == '+' || c == '-' || \
			 c == '/' || c == '=' || c == '_')
#define qpspecial(c) (c < ' ' || c == '=' || c == '?' || c == '_')

#define base64len(n) ((((n) + 2) / 3 ) * 4)	/* String len to base64 len */
#define strbase64(n) ((n) / 4 * 3)		/* Chars that fit in base64 */

#define ENCODELINELIMIT	76

static void unfold_header(char **, int);
static int field_encode_address(const char *, char **, int, const char *);
static int field_encode_quoted(const char *, char **, const char *, int,
			       int, int);
static int field_encode_base64(const char *, char **, const char *);
static int scanstring(const char *, int *, int *, int *);
static int utf8len(const char *);
static int pref_encoding(int, int, int);

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
	if (isascii((unsigned char) *p)) {
	    asciicount++;
	    if (qpspecial((unsigned char) *p))
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

    case CE_BASE64:
    	return field_encode_base64(name, value, charset);

    case CE_QUOTED:
	return field_encode_quoted(name, value, charset, asciicount,
				   eightbitcount + qpspecialcount, 0);

    default:
    	advise(NULL, "Internal error: unknown RFC-2047 encoding type");
	return 1;
    }
}

/*
 * Encode our specified header (or field) using quoted-printable
 */

static int
field_encode_quoted(const char *name, char **value, const char *charset,
		    int ascii, int encoded, int phraserules)
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
	 *
	 * Note that we have a different set of rules if we're processing
	 * RFC 5322 'phrase' (something you'd see in an address header).
	 */

	column++;

	if (*p == ' ') {
	    *q++ = '_';
	    ascii--;
	} else if (isascii((unsigned char) *p) &&
		   (phraserules ? qphrasevalid((unsigned char) *p) :
		   			!qpspecial((unsigned char) *p))) {
	    *q++ = *p;
	    ascii--;
	} else {
	    snprintf(q, outlen - (q - output), "=%02X", (unsigned char) *p);
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

    strcat(q, "?=");

    if (prefixlen)
    	strcat(q, "\n");

    free(*value);

    *value = output;

    return 0;
}

/*
 * Encode our specified header (or field) using base64.
 *
 * This is a little easier since every character gets encoded, we can
 * calculate the line wrap up front.
 */

static int
field_encode_base64(const char *name, char **value, const char *charset)
{
    int prefixlen = name ? strlen(name) + 2 : 0, charsetlen = strlen(charset);
    int outlen = 0, numencode, curlen;
    char *output = NULL, *p = *value, *q = NULL, *linestart;

    /*
     * Skip over any leading white space.
     */

    while (*p == ' ' || *p == '\t')
    	p++;

    /*
     * If we had a zero-length prefix, then just encode the whole field
     * as-is, without line wrapping.  Note that in addition to the encoding
     *
     * The added length we need is =? + charset + ?B? ... ?=
     *
     * That's 7 + strlen(charset) + 2 (for \n NUL).
     */

    while (prefixlen && ((base64len(strlen(p)) + 7 + charsetlen +
    			  prefixlen) > ENCODELINELIMIT)) {

	/*
	 * Our very first time, don't pad the line in the front
	 *
	 * Note ENCODELINELIMIT is + 2 because of \n \0
	 */


	if (! output) {
	    outlen += ENCODELINELIMIT + 2;
	    output = q = mh_xmalloc(outlen);
	    linestart = q - prefixlen;	/* Yes, this is intentional */
	} else {
	    int curstart = linestart - output;
	    curlen = q - output;

	    outlen += ENCODELINELIMIT + 2;
	    output = mh_xrealloc(output, outlen);
	    q = output + curlen;
	    linestart = output + curstart;
	}

	/*
	 * We should have enough space now, so prepend the encoding markers
	 * and character set information.  The leading space is intentional.
	 */

	q += snprintf(q, outlen - (q - output), " =?%s?B?", charset);

	/*
         * Find out how much room we have left on the line and see how
         * many characters we can stuff in.  The start of our line
         * is marked by "linestart", so use that to figure out how
         * many characters are left out of ENCODELINELIMIT.  Reserve
         * 2 characters for the end markers and calculate how many
         * characters we can fit into that space given the base64
         * encoding expansion.
	 */

	numencode = strbase64(ENCODELINELIMIT - (q - linestart) - 2);

	if (numencode <= 0) {
	    advise(NULL, "Internal error: tried to encode %d characters "
	    	   "in base64", numencode);
	    return 1;
	}

	/*
	 * RFC 2047 prohibits spanning multibyte characters across tokens.
	 * Right now we only check for UTF-8.
	 *
	 * So note the key here ... we want to make sure the character BEYOND
	 * our last character is not a continuation byte.  If it's the start
	 * of a new multibyte character or a single-byte character, that's ok.
	 */

	if (strcasecmp(charset, "UTF-8") == 0) {
	    /*
	     * p points to the start of our current buffer, so p + numencode
	     * is one past the last character to encode
	     */

	    while (numencode > 0 && ((*(p + numencode) & 0xc0) == 0x80))
	    	numencode--;

	    if (numencode == 0) {
	    	advise(NULL, "Internal error: could not find start of "
		       "UTF-8 character when base64 encoding header");
		return 1;
	    }
	}

	if (writeBase64raw((unsigned char *) p, numencode,
			   (unsigned char *) q) != OK) {
	    advise(NULL, "Internal error: base64 encoding of header failed");
	    return 1;
	}

	p += numencode;
	q += base64len(numencode);

	/*
	 * This will point us at the beginning of the new line (trust me).
	 */

	linestart = q + 3;

	/*
	 * What's going on here?  Well, we know we're continuing to the next
	 * line, so we want to add continuation padding.  We also add the
	 * trailing marker for the RFC 2047 token at this time as well.
	 * This uses a trick of snprintf(); we tell it to print a zero-length
	 * string, but pad it out to prefixlen - 1 characters; that ends
	 * up always printing out the requested number of spaces.  We use
	 * prefixlen - 1 because we always add a space on the starting
	 * token marker; this makes things work out correctly for the first
	 * line, which should have a space between the ':' and the start
	 * of the token.
	 *
	 * It's okay if you don't follow all of that.
	 */

	q += snprintf(q, outlen - (q - output), "?=\n%*s", prefixlen - 1, "");
    }

    /*
     * We're here if there is either no prefix, or we can fit it in less
     * than ENCODELINELIMIT characters.  Encode the whole thing.
     */

    outlen += prefixlen + 9 + charsetlen + base64len(strlen(p));
    curlen = q - output;

    output = mh_xrealloc(output, outlen);
    q = output + curlen;

    q += snprintf(q, outlen - (q - output), "%s=?%s?B?",
    		  prefixlen ? " " : "", charset);

    if (writeBase64raw((unsigned char *) p, strlen(p),
    		       (unsigned char *) q) != OK) {
	advise(NULL, "Internal error: base64 encoding of header failed");
	return 1;
    }

    strcat(q, "?=");

    if (prefixlen)
    	strcat(q, "\n");

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

    if (isascii((unsigned char) *p) || (((unsigned char) *p) & 0xc0) == 0x80)
    	return 0;

    p++;
    while ((((unsigned char) *p++) & 0xc0) == 0x80)
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

/*
 * Decode a header containing addresses.  This means we have to parse
 * each address and only encode the display-name or comment field.
 */

static int
field_encode_address(const char *name, char **value, int encoding,
		     const char *charset)
{
    int prefixlen = strlen(name) + 2, column = prefixlen, groupflag, errflag;
    int asciichars, specialchars, eightbitchars, reformat, len, retval;
    char *mp, *output = NULL;
    char *tmpbuf = NULL;
    size_t tmpbufsize = 0;
    struct mailname *mn;

    /*
     * Because these are addresses, we need to handle them individually.
     *
     * Break them down and process them one by one.  This means we have to
     * rewrite the whole header, but that's unavoidable.
     */

    /*
     * The output headers always have to start with a space first; this
     * is just the way the API works right now.
     */

    output = add(" ", output);

    for (groupflag = 0; (mp = getname(*value)); ) {
    	if ((mn = getm(mp, NULL, 0, AD_HOST, NULL)) == NULL) {
	    errflag++;
	    continue;
	}

	/*
	 * We only care if the phrase (m_pers) or any trailing comment
	 * (m_note) have 8-bit characters.  If doing q-p, we also need
	 * to encode anything marked as qspecial().  Unquote it first
	 * so the specialchars count is right.
	 */

	if ((len = strlen(mn->m_pers)) + 1 > tmpbufsize) {
	    tmpbuf = mh_xrealloc(tmpbuf, tmpbufsize = len + 1);
	}

	unquote_string(mn->m_pers, tmpbuf);

	if (scanstring(tmpbuf, &asciichars, &eightbitchars,
		       &specialchars)) {
		/*
		 * If we have 8-bit characters, encode it.
		 */

	    if (encoding == CE_UNKNOWN)
	    	encoding = prefencoding(asciichars, specialchars,
					eightbitchars);

	    strcpy(mn->m_pers, tmpbuf);

	    switch (encoding) {

	    case CE_BASE64:
	    	retval = field_encode_base64(NULL, &mn->m_pers, charset);
		break;

	    case CE_QUOTED:
	    	retval = field_encode_quoted(NULL, &mn->m_pers, charset,
					     asciichars,
					     eightbitchars + specialchars, 1);
		break;

	    default:
		advise(NULL, "Internal error: unknown RFC-2047 encoding type");
		return 1;
	    }
	}
    }
}

/*
 * Scan a string, check for characters that need to be encoded
 */

static int
scanstring(const char *string, int *asciilen, int *eightbitchars,
	   int *specialchars)
{
    *asciilen = 0;
    *eightbitchars = 0;
    *specialchars = 0;

    for (; *string != '\0'; string++) {
    	if ((isascii((unsigned char) *string))) {
	    (*asciilen++);
	    if (!qphrasevalid((unsigned char) *string))
	    	(*specialchars)++;
	} else {
	    (*eightbitchars)++;
	}
    }

    return eightbitchars > 0;
}

/*
 * This function is to be used to decide which encoding algorithm we should
 * use if one is not given.  Basically, we pick whichever one is the shorter
 * of the two.
 *
 * Arguments are:
 *
 * ascii	- Number of ASCII characters in to-be-encoded string.
 * specials	- Number of ASCII characters in to-be-encoded string that
 *		  still require encoding under quoted-printable.  Note that
 *		  these are included in the "ascii" total.
 * eightbit	- Eight-bit characters in the to-be-encoded string.
 *
 * Returns one of CE_BASE64 or CE_QUOTED.
 */

static int
prefencoding(int ascii, int specials, int eightbits)
{
    /*
     * The length of the q-p encoding is:
     *
     * ascii - specials + (specials + eightbits) * 3.
     *
     * The length of the base64 encoding is:
     *
     * base64len(ascii + eightbits)	(See macro for details)
     */

    return base64len(ascii + eightbits) < (ascii - specials +
    			(specials + eightbits) * 3) ? CE_BASE64 : CE_QUOTED;
}
