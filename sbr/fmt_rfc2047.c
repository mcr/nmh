
/*
 * fmt_rfc2047.c -- decode RFC-2047 header format 
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#ifdef HAVE_ICONV
#  include <iconv.h>
#  include <errno.h>
#endif

static signed char hexindex[] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

static signed char index_64[128] = {
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

static int
unqp (unsigned char byte1, unsigned char byte2)
{
    if (hexindex[byte1] == -1 || hexindex[byte2] == -1)
	return -1;
    return (hexindex[byte1] << 4 | hexindex[byte2]);
}

/* Check if character is linear whitespace */
#define is_lws(c)  ((c) == ' ' || (c) == '\t' || (c) == '\n')


/*
 * Decode the string as a RFC-2047 header field
 */

int
decode_rfc2047 (char *str, char *dst) 
{
    char *p, *q, *pp;
    char *startofmime, *endofmime;
    int c, quoted_printable;
    int encoding_found = 0;	/* did we decode anything?                */
    int between_encodings = 0;	/* are we between two encodings?          */
    int equals_pending = 0;	/* is there a '=' pending?                */
    int whitespace = 0;		/* how much whitespace between encodings? */
#ifdef HAVE_ICONV
    int use_iconv = 0;          /* are we converting encoding with iconv? */
    iconv_t cd;
    int fromutf8;
    char *saveq, *convbuf;
#endif

    if (!str)
	return 0;

    /*
     * Do a quick and dirty check for the '=' character.
     * This should quickly eliminate many cases.
     */
    if (!strchr (str, '='))
	return 0;

    for (p = str, q = dst; *p; p++) {

        /* reset iconv */
#ifdef HAVE_ICONV
        if (use_iconv) {
	    iconv_close(cd);
	    use_iconv = 0;
        }
#endif
	/*
	 * If we had an '=' character pending from
	 * last iteration, then add it first.
	 */
	if (equals_pending) {
	    *q++ = '=';
	    equals_pending = 0;
	    between_encodings = 0;	/* we have added non-whitespace text */
	}

	if (*p != '=') {
	    /* count linear whitespace while between encodings */
	    if (between_encodings && is_lws(*p))
		whitespace++;
	    else
		between_encodings = 0;	/* we have added non-whitespace text */
	    *q++ = *p;
	    continue;
	}

	equals_pending = 1;	/* we have a '=' pending */

	/* Check for initial =? */
	if (*p == '=' && p[1] && p[1] == '?' && p[2]) {
	    startofmime = p + 2;

	    /* Scan ahead for the next '?' character */
	    for (pp = startofmime; *pp && *pp != '?'; pp++)
		;

	    if (!*pp)
		continue;

	    /* Check if character set can be handled natively */
	    if (!check_charset(startofmime, pp - startofmime)) {
#ifdef HAVE_ICONV
	        /* .. it can't. We'll use iconv then. */
		*pp = '\0';
	        cd = iconv_open(get_charset(), startofmime);
		fromutf8 = !strcasecmp(startofmime, "UTF-8");
		*pp = '?';
                if (cd == (iconv_t)-1) continue;
		use_iconv = 1;
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
		if (is_lws(*pp)) {
		    break;
		} else if (*pp == '?' && pp[1] == '=') {
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
	    equals_pending = 0;

	    /*
	     * If we are between two encoded words separated only by
	     * linear whitespace, then we ignore the whitespace.
	     * We will roll back the buffer the number of whitespace
	     * characters we've seen since last encoded word.
	     */
	    if (between_encodings)
		q -= whitespace;

#ifdef HAVE_ICONV
	    if (use_iconv) {
	        saveq = q;
		if (!(q = convbuf = (char *)malloc(endofmime - startofmime)))
		    continue;
            }
#endif

	    /* Now decode the text */
	    if (quoted_printable) {
		for (pp = startofmime; pp < endofmime; pp++) {
		    if (*pp == '=') {
			c = unqp (pp[1], pp[2]);
			if (c == -1)
			    continue;
			if (c != 0)
			    *q++ = c;
			pp += 2;
		    } else if (*pp == '_') {
			*q++ = ' ';
		    } else {
			*q++ = *pp;
		    }
		}
	    } else {
		/* base64 */
		int c1, c2, c3, c4;

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
			*q++ = (c1 << 2) | (c2 >> 4);
			pp++;
		    }
		    /* 4 + 4 bits */
		    while ((pp < endofmime) &&
			   ((c3 = char64(*pp)) == -1)) {
			pp++;
		    }
		    if (pp < endofmime && c2 != -1 && c3 != -1) {
			*q++ = ((c2 & 0xF) << 4) | (c3 >> 2);
			pp++;
		    }
		    /* 2 + 6 bits */
		    while ((pp < endofmime) &&
			   ((c4 = char64(*pp)) == -1)) {
			pp++;
		    }
		    if (pp < endofmime && c3 != -1 && c4 != -1) {
			*q++ = ((c3 & 0x3) << 6) | (c4);
			pp++;
		    }
		}
	    }

#ifdef HAVE_ICONV
            /* Convert to native character set */
	    if (use_iconv) {
		size_t inbytes = q - convbuf;
		size_t outbytes = BUFSIZ;
		ICONV_CONST char *start = convbuf;
		
		while (inbytes) {
		    if (iconv(cd, &start, &inbytes, &saveq, &outbytes) ==
		            (size_t)-1) {
			if (errno != EILSEQ) break;
			/* character couldn't be converted. we output a `?'
			 * and try to carry on which won't work if
			 * either encoding was stateful */
			iconv (cd, 0, 0, &saveq, &outbytes);
			*saveq++ = '?';
                        /* skip to next input character */
			if (fromutf8) {
			    for (start++;(*start & 192) == 128;start++)
			        inbytes--;
			} else
			    start++, inbytes--;
		    }
		}
		q = saveq;
		free(convbuf);
	    }
#endif
	    
	    /*
	     * Now that we are done decoding this particular
	     * encoded word, advance string to trailing '='.
	     */
	    p = endofmime + 1;

	    encoding_found = 1;		/* we found (at least 1) encoded word */
	    between_encodings = 1;	/* we have just decoded something     */
	    whitespace = 0;		/* re-initialize amount of whitespace */
	}
    }
#ifdef HAVE_ICONV
    if (use_iconv) iconv_close(cd);
#endif

    /* If an equals was pending at end of string, add it now. */
    if (equals_pending)
	*q++ = '=';
    *q = '\0';

    return encoding_found;
}
