
/*
 * vmhsbr.c -- routines to help vmh along
 *
 * $Id$
 */

/*
 * TODO (for vrsn 2):
 *     INI: include width of windows
 */

#include <h/mh.h>
#include <h/vmhsbr.h>
#include <errno.h>

static char *types[] = {
    "OK",
    "INI", "ACK", "ERR", "CMD", "QRY", "TTY", "WIN", "DATA", "EOF", "FIN",
    "XXX", NULL
};

static	FILE *fp = NULL;

static int PEERrfd = NOTOK;
static int PEERwfd = NOTOK;

/*
 * static prototypes
 */
static int rclose (struct record *, char *, ...);


int
rcinit (int rfd, int wfd)
{
    char *cp, buffer[BUFSIZ];

    PEERrfd = rfd;
    PEERwfd = wfd;

    if ((cp = getenv ("MHVDEBUG")) && *cp) {
	snprintf (buffer, sizeof(buffer), "%s.out", invo_name);
	if ((fp = fopen (buffer, "w"))) {
	  fseek (fp, 0L, SEEK_END);
	  fprintf (fp, "%d: rcinit (%d, %d)\n", (int) getpid(), rfd, wfd);
	  fflush (fp);
	}
    }

    return OK;
}


int
rcdone (void)
{
    if (PEERrfd != NOTOK)
	close (PEERrfd);
    if (PEERwfd != NOTOK)
	close (PEERwfd);

    if (fp) {
	fclose (fp);
	fp = NULL;
    }
    return OK;
}


int
rc2rc (char code, int len, char *data, struct record *rc)
{
    if (rc2peer (code, len, data) == NOTOK)
	return NOTOK;

    return peer2rc (rc);
}


int
str2rc (char code, char *str, struct record *rc)
{
    return rc2rc (code, str ? strlen (str) : 0, str, rc);
}


int
peer2rc (struct record *rc)
{
    if (rc->rc_data)
	free (rc->rc_data);

    if (read (PEERrfd, (char *) rc_head (rc), RHSIZE (rc)) != RHSIZE (rc))
	return rclose (rc, "read from peer lost(1)");
    if (rc->rc_len) {
	if ((rc->rc_data = malloc ((unsigned) rc->rc_len + 1)) == NULL)
	    return rclose (rc, "malloc of %d lost", rc->rc_len + 1);
	if (read (PEERrfd, rc->rc_data, rc->rc_len) != rc->rc_len)
	    return rclose (rc, "read from peer lost(2)");
	rc->rc_data[rc->rc_len] = 0;
    }
    else
	rc->rc_data = NULL;

    if (fp) {
	fseek (fp, 0L, SEEK_END);
	fprintf (fp, "%d: <--- %s %d: \"%*.*s\"\n", (int) getpid(),
		types[(unsigned char)rc->rc_type], rc->rc_len,
		rc->rc_len, rc->rc_len, rc->rc_data);
	fflush (fp);
    }

    return rc->rc_type;
}


int
rc2peer (char code, int len, char *data)
{
    struct record   rcs;
    register struct record *rc = &rcs;

    rc->rc_type = code;
    rc->rc_len = len;

    if (fp) {
	fseek (fp, 0L, SEEK_END);
	fprintf (fp, "%d: ---> %s %d: \"%*.*s\"\n", (int) getpid(),
		types[(unsigned char)rc->rc_type], rc->rc_len,
		rc->rc_len, rc->rc_len, data);
	fflush (fp);
    }

    if (write (PEERwfd, (char *) rc_head (rc), RHSIZE (rc)) != RHSIZE (rc))
	return rclose (rc, "write to peer lost(1)");

    if (rc->rc_len)
	if (write (PEERwfd, data, rc->rc_len) != rc->rc_len)
	    return rclose (rc, "write to peer lost(2)");

    return OK;
}


int
str2peer (char code, char *str)
{
    return rc2peer (code, str ? strlen (str) : 0, str);
}


int
fmt2peer (char code, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    return verr2peer (code, NULL, fmt, ap);
    va_end(ap);
}


int
err2peer (char code, char *what, char *fmt, ...)
{
    int     return_value;
    va_list ap;

    va_start(ap, fmt);
    return_value = verr2peer(code, what, fmt, ap);
    va_end(ap);
    return return_value;  /* This routine returned garbage before 1999-07-15. */
}


int
verr2peer (char code, char *what, char *fmt, va_list ap)
{
    int eindex = errno;
    int len, buflen;
    char *bp, *s, buffer[BUFSIZ * 2];

    /* Get buffer ready to go */
    bp = buffer;
    buflen = sizeof(buffer);

    vsnprintf (bp, buflen, fmt, ap);
    len = strlen (bp);
    bp += len;
    buflen -= len;

    if (what) {
	if (*what) {
	    snprintf (bp, buflen, " %s: ", what);
	    len = strlen (bp);
	    bp += len;
	    buflen -= len;
	}
	if ((s = strerror (eindex)))
	    strncpy (bp, s, buflen);
	else
	    snprintf (bp, buflen, "unknown error %d", eindex);
	len = strlen (bp);
	bp += len;
	buflen -= len;
    }

    return rc2peer (code, bp - buffer, buffer);
}


static int
rclose (struct record *rc, char *fmt, ...)
{
    va_list ap;
    static char buffer[BUFSIZ * 2];

    va_start(ap, fmt);
    vsnprintf (buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    rc->rc_len = strlen (rc->rc_data = getcpy (buffer));
    return (rc->rc_type = RC_XXX);
}
