/* cpydgst.c -- copy from one fd to another in encapsulating mode
 *           -- (do dashstuffing of input data).
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "cpydgst.h"
#include "error.h"

/*
 * We want to perform the substitution
 *
 *     \n(-.*)\n      -->       \n- \1\n
 *
 * This is equivalent to the sed substitution
 *
 *     sed -e 's%^-%- -%' < ifile > ofile
 *
 *  but the routine below is faster than the pipe, fork, and exec.
 */

#define	S1 0
#define	S2 1

#define	output(c)   if (bp >= dp) {flush(); *bp++ = c;} else *bp++ = c
#define	flush()	    if ((j = bp - outbuf) && write (out, outbuf, j) != j) \
			adios (ofile, "error writing"); \
		    else \
			bp = outbuf


void
cpydgst (int in, int out, char *ifile, char *ofile)
{
    int i, j, state;
    char *cp, *ep;
    char *bp, *dp;
    char buffer[BUFSIZ], outbuf[BUFSIZ];

    dp = (bp = outbuf) + sizeof outbuf;
    for (state = S1; (i = read (in, buffer, sizeof buffer)) > 0;)
	for (ep = (cp = buffer) + i; cp < ep; cp++) {
	    if (*cp == '\0')
		continue;
	    switch (state) {
		case S1: 
		    if (*cp == '-') {
			output ('-');
			output (' ');
		    }
		    state = S2;
		    /* FALLTHRU */
		case S2: 
		    output (*cp);
		    if (*cp == '\n')
			state = S1;
		    break;
	    }
	}

    if (i == -1)
	adios (ifile, "error reading");
    flush();
}
