/*
 * base64.c -- routines for converting to base64
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/mime.h>

static char nib2b64[0x40+1] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int
writeBase64aux (FILE *in, FILE *out, int crlf)
{
    unsigned int cc, n;
    unsigned char inbuf[3];

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
		if (inbuf[i] == '\n') {
		    inbuf[i] = '\r';
		    /*
		     * If it's the last character in the buffer, we can just
		     * substitute a \r and push a \n back.  Otherwise shuffle
		     * everything down and push the last character back.
		     */
		    if (i == cc - 1) {
			ungetc('\n', in);
		    } else {
		    	/* This only works as long as sizeof(inbuf) == 3 */
			ungetc(inbuf[cc - 1], in);
			if (cc == 3 && i == 0)
			    inbuf[2] = inbuf[1];
			inbuf[++i] = '\n';
		    }
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

	fwrite (outbuf, sizeof(*outbuf), sizeof(outbuf), out);

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
writeBase64 (unsigned char *in, size_t length, unsigned char *out)
{
    unsigned int n = BPERLIN;

    while (1) {
	unsigned long bits;
	unsigned char *bp;
	unsigned int cc;
	for (cc = 0, bp = in; length > 0 && cc < 3; ++cc, ++bp, --length)
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
writeBase64raw (unsigned char *in, size_t length, unsigned char *out)
{
    while (1) {
	unsigned long bits;
	unsigned char *bp;
	unsigned int cc;
	for (cc = 0, bp = in; length > 0 && cc < 3; ++cc, ++bp, --length)
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
