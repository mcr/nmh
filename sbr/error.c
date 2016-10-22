
/*
 * error.c -- main error handling routines
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

#include <sys/types.h>
#include <sys/uio.h>


/* advise calls advertise() with no tail to print fmt, and perhaps what,
 * to stderr. */
void
advise (const char *what, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    advertise (what, NULL, fmt, ap);
    va_end(ap);
}


/* adios calls advertise() with no tail to print fmt, and perhaps what,
 * to stderr, and "ends" the program with an error exit status.  The
 * route to exit is via the done function pointer and may not be
 * straightforward.
 * FIXME: Document if this function can ever return.  If not, perhaps an
 * abort(3) at the end of the routine would make that more clear. */
void
adios (const char *what, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    advertise (what, NULL, fmt, ap);
    va_end(ap);
    done (1);
}


/* admonish calls advertise() with a tail indicating the program
 * continues. */
void
admonish (char *what, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    advertise (what, "continuing...", fmt, ap);
    va_end(ap);
}


/* advertise prints fmt and ap to stderr after flushing stdout.
 * If invo_name isn't NULL or empty, it precedes the output seperated by ": ".
 * If what isn't NULL, errno as a string is appended.
 * A non-empty what separates fmt from errno, surrounded by " " and ": ".
 * BUG: No space separator before errno if what is "".
 * If tail isn't NULL or empty then ", " and tail are appended
 * before the finishing "\n".
 * In summary: "invo_name: fmt what: errno, tail\n". */
void
advertise (const char *what, char *tail, const char *fmt, va_list ap)
{
    int	eindex = errno;
    char buffer[BUFSIZ], err[BUFSIZ];
    struct iovec iob[20], *iov;

    fflush (stdout);

    fflush (stderr);
    iov = iob;

    if (invo_name && *invo_name) {
	iov->iov_len = strlen (iov->iov_base = invo_name);
	iov++;
	iov->iov_len = strlen (iov->iov_base = ": ");
	iov++;
    }
    
    vsnprintf (buffer, sizeof(buffer), fmt, ap);
    iov->iov_len = strlen (iov->iov_base = buffer);
    iov++;
    if (what) {
	if (*what) {
	    iov->iov_len = strlen (iov->iov_base = " ");
	    iov++;
	    iov->iov_len = strlen (iov->iov_base = (void*)what);
	    iov++;
	    iov->iov_len = strlen (iov->iov_base = ": ");
	    iov++;
	}
        if (!(iov->iov_base = strerror (eindex))) {
	    /* this shouldn't happen, but we'll test for it just in case */
	    snprintf (err, sizeof(err), "Error %d", eindex);
	    iov->iov_base = err;
	}
	iov->iov_len = strlen (iov->iov_base);
	iov++;
    }
    if (tail && *tail) {
	iov->iov_len = strlen (iov->iov_base = ", ");
	iov++;
	iov->iov_len = strlen (iov->iov_base = tail);
	iov++;
    }
    iov->iov_len = strlen (iov->iov_base = "\n");
    iov++;
    if (writev (fileno (stderr), iob, iov - iob) < 0) {
        snprintf(buffer, sizeof buffer, "%s: write stderr failed: %d\n",
            invo_name && *invo_name ? invo_name : "nmh", errno);
        if (write(2, buffer, strlen(buffer)) == -1) {
            /* Ignore.  if-statement needed to shut up compiler. */
        }
    }
}
