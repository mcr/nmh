/*
 * base64.c -- routines for converting to base64
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/mime.h>
#include <h/md5.h>
#include <inttypes.h>

static char nib2b64[0x40+1] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int
writeBase64aux (FILE *in, FILE *out, int crlf)
{
    unsigned int cc, n;
    unsigned char inbuf[3];
    int skipnl = 0;

    n = BPERLIN;
    while ((cc = fread (inbuf, sizeof(*inbuf), sizeof(inbuf), in)) > 0) {
	unsigned long bits;
	char *bp;
	char outbuf[4];

	if (cc < sizeof(inbuf)) {
	    inbuf[2] = 0;
	    if (cc < sizeof(inbuf) - 1)
		inbuf[1] = 0;
	}

	/*
	 * Convert a LF to a CRLF if desired.  That means we need to push
	 * data back into the input stream.
	 */

	if (crlf) {
	    unsigned int i;

	    for (i = 0; i < cc; i++) {
		if (inbuf[i] == '\n' && !skipnl) {
		    inbuf[i] = '\r';
		    /*
		     * If it's the last character in the buffer, we can just
		     * substitute a \r and push a \n back.  Otherwise shuffle
		     * everything down and push the last character back.
		     */
		    if (i == cc - 1) {
		    	/*
			 * If we're at the end of the input, there might be
			 * more room in inbuf; if so, add it there.  Otherwise
			 * push it back to the input.
			 */
		    	if (cc < sizeof(inbuf))
			    inbuf[cc++] = '\n';
			else
			    ungetc('\n', in);
			skipnl = 1;
		    } else {
		    	/* This only works as long as sizeof(inbuf) == 3 */
			ungetc(inbuf[cc - 1], in);
			if (cc == 3 && i == 0)
			    inbuf[2] = inbuf[1];
			inbuf[++i] = '\n';
		    }
		} else {
		    skipnl = 0;
		}
	    }
	}

	bits = (inbuf[0] & 0xff) << 16;
	bits |= (inbuf[1] & 0xff) << 8;
	bits |= inbuf[2] & 0xff;

	for (bp = outbuf + sizeof(outbuf); bp > outbuf; bits >>= 6)
	    *--bp = nib2b64[bits & 0x3f];
	if (cc < sizeof(inbuf)) {
	    outbuf[3] = '=';
	    if (cc < sizeof inbuf - 1)
		outbuf[2] = '=';
	}

	if (fwrite (outbuf, sizeof(*outbuf), sizeof(outbuf), out) <
            sizeof outbuf) {
	    advise ("writeBase64aux", "fwrite");
	}

	if (cc < sizeof(inbuf)) {
	    putc ('\n', out);
	    return OK;
	}

	if (--n <= 0) {
	    n = BPERLIN;
	    putc ('\n', out);
	}
    }
    if (n != BPERLIN)
	putc ('\n', out);

    return OK;
}


/* Caller is responsible for ensuring that the out array is long
   enough.  Given length is that of in, out should be have at
   least this capacity:
     4 * [length/3]  +  length/57  +  2
   But double the length will certainly be sufficient. */
int
writeBase64 (const unsigned char *in, size_t length, unsigned char *out)
{
    unsigned int n = BPERLIN;

    while (1) {
	unsigned long bits;
	unsigned char *bp;
	unsigned int cc;
	for (cc = 0; length > 0 && cc < 3; ++cc, --length)
          /* empty */ ;

	if (cc == 0) {
	    break;
	} else {
	    bits = (in[0] & 0xff) << 16;
	    if (cc > 1) {
		bits |= (in[1] & 0xff) << 8;
		if (cc > 2) {
		    bits |= in[2] & 0xff;
		}
	    }
	}

	for (bp = out + 4; bp > out; bits >>= 6)
	    *--bp = nib2b64[bits & 0x3f];
	if (cc < 3) {
	    out[3] = '=';
	    if (cc < 2)
		out[2] = '=';
	    out += 4;
	    n = 0;
	    break;
	}

	in += 3;
	out += 4;
	if (--n <= 0) {
	    n = BPERLIN;
	    *out++ = '\n';
	}
    }
    if (n != BPERLIN)
	*out++ = '\n';

    *out = '\0';

    return OK;
}

/*
 * Essentially a duplicate of writeBase64, but without line wrapping or
 * newline termination (note: string IS NUL terminated)
 */

int
writeBase64raw (const unsigned char *in, size_t length, unsigned char *out)
{
    while (1) {
	unsigned long bits;
	unsigned char *bp;
	unsigned int cc;
	for (cc = 0; length > 0 && cc < 3; ++cc, --length)
          /* empty */ ;

	if (cc == 0) {
	    break;
	} else {
	    bits = (in[0] & 0xff) << 16;
	    if (cc > 1) {
		bits |= (in[1] & 0xff) << 8;
		if (cc > 2) {
		    bits |= in[2] & 0xff;
		}
	    }
	}

	for (bp = out + 4; bp > out; bits >>= 6)
	    *--bp = nib2b64[bits & 0x3f];
	if (cc < 3) {
	    out[3] = '=';
	    if (cc < 2)
		out[2] = '=';
	    out += 4;
	    break;
	}

	in += 3;
	out += 4;
    }

    *out = '\0';

    return OK;
}


static unsigned char b642nib[0x80] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff
};

/*
 * Decode a base64 string.  The result, decoded, must be freed by the caller.
 * See description of arguments with declaration in h/prototypes.h.
 */
int
decodeBase64 (const char *encoded, const char **decoded, size_t *len, int skip_crs,
              unsigned char *digest) {
    const char *cp = encoded;
    int self_delimiting = 0;
    int bitno, skip;
    uint32_t bits;
    /* Size the decoded string very conservatively. */
    charstring_t decoded_c = charstring_create (strlen (encoded));
    MD5_CTX mdContext;

    if (digest) { MD5Init (&mdContext); }

    bitno = 18;
    bits = 0L;
    skip = 0;

    for (; *cp; ++cp) {
        switch (*cp) {
            unsigned char value;

            default:
                if (isspace ((unsigned char) *cp)) {
                    break;
                }
                if (skip  ||  (((unsigned char) *cp) & 0x80)  ||
                    (value = b642nib[((unsigned char) *cp) & 0x7f]) > 0x3f) {
                    advise (NULL, "invalid BASE64 encoding in %s", encoded);
                    charstring_free (decoded_c);
                    *decoded = NULL;

                    return NOTOK;
                }

                bits |= value << bitno;
test_end:
                if ((bitno -= 6) < 0) {
                    char b = (char) ((bits >> 16) & 0xff);

                    if (! skip_crs  ||  b != '\r') {
                        charstring_push_back (decoded_c, b);
                    }
                    if (digest) { MD5Update (&mdContext, (unsigned char *) &b, 1); }
                    if (skip < 2) {
                        b = (bits >> 8) & 0xff;
                        if (! skip_crs  ||  b != '\r') {
                            charstring_push_back (decoded_c, b);
                        }
                        if (digest) { MD5Update (&mdContext, (unsigned char *) &b, 1); }
                        if (skip < 1) {
                            b = bits & 0xff;
                            if (! skip_crs  ||  b != '\r') {
                                charstring_push_back (decoded_c, b);
                            }
                            if (digest) { MD5Update (&mdContext, (unsigned char *) &b, 1); }
                        }
                    }

                    bitno = 18;
                    bits = 0L;
                    skip = 0;
                }
                break;

            case '=':
                if (++skip > 3) {
                    self_delimiting = 1;
                    break;
                } else {
                    goto test_end;
                }
        }
    }

    if (! self_delimiting  &&  bitno != 18) {
        advise (NULL, "premature ending (bitno %d) in %s", bitno, encoded);
        charstring_free (decoded_c);
        *decoded = NULL;

        return NOTOK;
    }

    *decoded = charstring_buffer_copy (decoded_c);
    *len = charstring_bytes (decoded_c);
    charstring_free (decoded_c);

    if (digest) {
        MD5Final (digest, &mdContext);
    }

    return OK;
}


/*
 * Prepare an unsigned char array for display by replacing non-printable
 * ASCII bytes with their hex representation.  Assumes ASCII input.  output
 * is allocated by the function and must be freed by the caller.
 */
void
hexify (const unsigned char *input, size_t len, char **output) {
    /* Start with a charstring capacity that's arbitrarily larger than len. */
    const charstring_t tmp = charstring_create (2 * len);
    const unsigned char *cp = input;
    size_t i;

    for (i = 0; i < len; ++i, ++cp) {
        if (isascii(*cp) && isprint(*cp)) {
            charstring_push_back (tmp, (const char) *cp);
        } else {
            char s[16];
            const int num = snprintf(s, sizeof s, "[0x%02x]", *cp);

            if (num <= 0  ||  (unsigned int) num >= sizeof s) {
                advise (NULL, "hexify failed to write nonprintable character, needed %d bytes", num + 1);
            } else {
                charstring_append_cstring (tmp, s);
            }
        }
    }

    *output = charstring_buffer_copy (tmp);
    charstring_free (tmp);
}
