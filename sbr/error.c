
/*
 * error.c -- main error handling routines
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

#ifdef HAVE_WRITEV
# include <sys/types.h>
# include <sys/uio.h>
#endif

#include <errno.h>


/*
 * print out error message
 */
void
advise (char *what, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    advertise (what, NULL, fmt, ap);
    va_end(ap);
}


/*
 * print out error message and exit
 */
void
adios (char *what, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    advertise (what, NULL, fmt, ap);
    va_end(ap);
    done (1);
}


/*
 * admonish the user
 */
void
admonish (char *what, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    advertise (what, "continuing...", fmt, ap);
    va_end(ap);
}


/*
 * main routine for printing error messages.
 *
 * Use writev() if available, for slightly better performance.
 * Why?  Well, there are a couple of reasons.  Primarily, it
 * gives a smoother output...  More importantly though, it's a
 * sexy syscall()...
 */
void
advertise (char *what, char *tail, char *fmt, va_list ap)
{
    int	eindex = errno;

#ifdef HAVE_WRITEV
    char buffer[BUFSIZ], err[BUFSIZ];
    struct iovec iob[20], *iov;
#endif

    fflush (stdout);

#ifdef HAVE_WRITEV
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
	    iov->iov_len = strlen (iov->iov_base = what);
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
    writev (fileno (stderr), iob, iov - iob);
#else
    if (invo_name && *invo_name)
	fprintf (stderr, "%s: ", invo_name);
    vfprintf (stderr, fmt, ap);

    if (what) {
	char *s;

	if (*what)
	    fprintf (stderr, " %s: ", what);
	if ((s = strerror(eindex)))
	    fprintf (stderr, "%s", s);
	else
	    fprintf (stderr, "Error %d", eindex);
    }
    if (tail)
	fprintf (stderr, ", %s", tail);
    fputc ('\n', stderr);
#endif
}
