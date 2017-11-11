/* fmt_rfc2047.c -- decode RFC-2047 header format 
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "check_charset.h"
#include "h/utils.h"
#ifdef HAVE_ICONV
#  include <iconv.h>
#endif

static const signed char hexindex[] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

static const signed char index_64[128] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};

#define char64(c) (((unsigned char) (c) > 127) ? -1 : index_64[(unsigned char) (c)])

/*
 * Decode two quoted-pair characters
 */

int
decode_qp (unsigned char byte1, unsigned char byte2)
{
    if (hexindex[byte1] == -1 || hexindex[byte2] == -1)
	return -1;
    return hexindex[byte1] << 4 | hexindex[byte2];
}

/* Check if character is linear whitespace */
#define is_lws(c)  ((c) == ' ' || (c) == '\t' || (c) == '\n')


/*
 * Decode the string as a RFC-2047 header field
 */

/* Add character to the destination buffer, and bomb out if it fills up */
#define ADDCHR(C) do { *q++ = (C); dstlen--; if (!dstlen) goto buffull; } while (0)

int
decode_rfc2047 (char *str, char *dst, size_t dstlen)
{
    char *p, *q, *pp;
    char *startofmime, *endofmime, *endofcharset;
    int c, quoted_printable;
    int encoding_found = 0;	/* did we decode anything?                */
    int whitespace = 0;		/* how much whitespace between encodings? */
#ifdef HAVE_ICONV
    iconv_t cd = NULL;
    int fromutf8 = 0;
    char *saveq, *convbuf = NULL;
    size_t savedstlen;
#endif

    if (!str)
	return 0;

    /*
     * Do a quick and dirty check for the '=' character.
     * This should quickly eliminate many cases.
     */
    if (!strchr (str, '='))
	return 0;

    bool use_iconv = false; /* are we converting encoding with iconv? */
    bool between_encodings = false;
    bool equals_pending = false;
    for (p = str, q = dst; *p; p++) {

        /* reset iconv */
#ifdef HAVE_ICONV
        if (use_iconv) {
	    iconv_close(cd);
	    use_iconv = false;
        }
#endif
	/*
	 * If we had an '=' character pending from
	 * last iteration, then add it first.
	 */
	if (equals_pending) {
	    ADDCHR('=');
	    equals_pending = false;
	    between_encodings = false;	/* we have added non-whitespace text */
	}

	if (*p != '=') {
	    /* count linear whitespace while between encodings */
	    if (between_encodings && is_lws(*p))
		whitespace++;
	    else
		between_encodings = false;	/* we have added non-whitespace text */
	    ADDCHR(*p);
	    continue;
	}

	equals_pending = true;

	/* Check for initial =? */
	if (*p == '=' && p[1] == '?' && p[2]) {
	    startofmime = p + 2;

	    /* Scan ahead for the next '?' character */
	    for (pp = startofmime; *pp && *pp != '?'; pp++)
		;

	    if (!*pp)
		continue;

	    /*
	     * RFC 2231 specifies that language information can appear
	     * in a charset specification like so:
	     *
	     * =?us-ascii*en?Q?Foo?=
	     *
	     * Right now we don't use language information, so ignore it.
	     */

	    for (endofcharset = startofmime;
	    		*endofcharset != '*' && endofcharset < pp;
							endofcharset++)
		;

	    /* Check if character set can be handled natively */
	    if (!check_charset(startofmime, endofcharset - startofmime)) {
#ifdef HAVE_ICONV
	        /* .. it can't. We'll use iconv then. */
		*endofcharset = '\0';
	        cd = iconv_open(get_charset(), startofmime);
		fromutf8 = !strcasecmp(startofmime, "UTF-8");
		*pp = '?';
                if (cd == (iconv_t)-1) continue;
		use_iconv = true;
#else
		continue;
#endif
	    }

	    startofmime = pp + 1;

	    /* Check for valid encoding type */
	    if (*startofmime != 'B' && *startofmime != 'b' &&
		*startofmime != 'Q' && *startofmime != 'q')
		continue;

	    /* Is encoding quoted printable or base64? */
	    quoted_printable = (*startofmime == 'Q' || *startofmime == 'q');
	    startofmime++;

	    /* Check for next '?' character */
	    if (*startofmime != '?')
		continue;
	    startofmime++;

	    /*
	     * Scan ahead for the ending ?=
	     *
	     * While doing this, we will also check if encoded
	     * word has any embedded linear whitespace.
	     */
	    endofmime = NULL;
	    for (pp = startofmime; *pp && *(pp+1); pp++) {
		if (is_lws(*pp))
		    break;
		if (*pp == '?' && pp[1] == '=') {
		    endofmime = pp;
		    break;
		}
	    }
	    if (is_lws(*pp) || endofmime == NULL)
		continue;

	    /*
	     * We've found an encoded word, so we can drop
	     * the '=' that was pending
	     */
	    equals_pending = false;

	    /*
	     * If we are between two encoded words separated only by
	     * linear whitespace, then we ignore the whitespace.
	     * We will roll back the buffer the number of whitespace
	     * characters we've seen since last encoded word.
	     */
	    if (between_encodings) {
		q -= whitespace;
		dstlen += whitespace;
	    }

#ifdef HAVE_ICONV
	    /*
	     * empty encoded text. This ensures that we don't
	     * malloc 0 bytes but skip on to the end
	     */
	    if (endofmime == startofmime && use_iconv) {
		use_iconv = false;
		iconv_close(cd);
            }

	    if (use_iconv) {
		saveq = q;
		savedstlen = dstlen;
                q = convbuf = mh_xmalloc(endofmime - startofmime);
            }
/* ADDCHR2 is for adding characters when q is or might be convbuf:
 * in this case on buffer-full we want to run iconv before returning.
 * I apologise for the dreadful name.
 */
#define ADDCHR2(C) do { *q++ = (C); dstlen--; if (!dstlen) goto iconvbuffull; } while (0)
#else
#define ADDCHR2(C) ADDCHR(C)
#endif

	    /* Now decode the text */
	    if (quoted_printable) {
		for (pp = startofmime; pp < endofmime; pp++) {
		    if (*pp == '=') {
			c = decode_qp (pp[1], pp[2]);
			if (c == -1)
			    continue;
			if (c != 0)
			    *q++ = c;
			pp += 2;
		    } else if (*pp == '_') {
			ADDCHR2(' ');
		    } else {
			ADDCHR2(*pp);
		    }
		}
	    } else {
		/* base64 */
		int c1, c2, c3, c4;
		c1 = c2 = c3 = c4 = -1;

		pp = startofmime;
		while (pp < endofmime) {
		    /* 6 + 2 bits */
		    while ((pp < endofmime) &&
			   ((c1 = char64(*pp)) == -1)) {
			pp++;
		    }
		    if (pp < endofmime) {
			pp++;
		    }
		    while ((pp < endofmime) &&
			   ((c2 = char64(*pp)) == -1)) {
			pp++;
		    }
		    if (pp < endofmime && c1 != -1 && c2 != -1) {
			ADDCHR2((c1 << 2) | (c2 >> 4));
			pp++;
		    }
		    /* 4 + 4 bits */
		    while ((pp < endofmime) &&
			   ((c3 = char64(*pp)) == -1)) {
			pp++;
		    }
		    if (pp < endofmime && c2 != -1 && c3 != -1) {
			ADDCHR2(((c2 & 0xF) << 4) | (c3 >> 2));
			pp++;
		    }
		    /* 2 + 6 bits */
		    while ((pp < endofmime) &&
			   ((c4 = char64(*pp)) == -1)) {
			pp++;
		    }
		    if (pp < endofmime && c3 != -1 && c4 != -1) {
			ADDCHR2(((c3 & 0x3) << 6) | (c4));
			pp++;
		    }
		}
	    }

#ifdef HAVE_ICONV
	iconvbuffull:
	    /* NB that the string at convbuf is not necessarily NUL terminated here:
	     * q points to the first byte after the valid part.
	     */
            /* Convert to native character set */
	    if (use_iconv) {
		size_t inbytes = q - convbuf;
		ICONV_CONST char *start = convbuf;
		
		while (inbytes) {
		    if (iconv(cd, &start, &inbytes, &saveq, &savedstlen) ==
		            (size_t)-1) {
			if (errno != EILSEQ) break;
			/* character couldn't be converted. we output a `?'
			 * and try to carry on which won't work if
			 * either encoding was stateful */
			iconv (cd, 0, 0, &saveq, &savedstlen);
			if (!savedstlen)
			    break;
			*saveq++ = '?';
			savedstlen--;
			if (!savedstlen)
			    break;
			/* skip to next input character */
			if (fromutf8) {
			    for (++start, --inbytes;
				 start < q  &&  (*start & 192) == 128;
				 ++start, --inbytes)
				continue;
			} else
			    start++, inbytes--;
			if (start >= q)
			    break;
		    }
		}
		q = saveq;
		/* Stop now if (1) we hit the end of the buffer trying to do
		 * MIME decoding and have just iconv-converted a partial string
		 * or (2) our iconv-conversion hit the end of the buffer.
		 */
		if (!dstlen || !savedstlen)
		    goto buffull;
		dstlen = savedstlen;
		free(convbuf);
	    }
#endif
	    
	    /*
	     * Now that we are done decoding this particular
	     * encoded word, advance string to trailing '='.
	     */
	    p = endofmime + 1;

	    encoding_found = 1;		/* we found (at least 1) encoded word */
	    between_encodings = true;	/* we have just decoded something     */
	    whitespace = 0;		/* re-initialize amount of whitespace */
	}
    }
#ifdef HAVE_ICONV
    if (use_iconv) iconv_close(cd);
#endif

    /* If an equals was pending at end of string, add it now. */
    if (equals_pending)
	ADDCHR('=');
    *q = '\0';

    return encoding_found;

  buffull:
    /* q is currently just off the end of the buffer, so rewind to NUL terminate */
    q--;
    *q = '\0';
    return encoding_found;
}
