
/*
 * mhparse.c -- misc routines to process MIME messages
 *
 * $Id$
 */

#include <h/mh.h>
#include <errno.h>
#include <h/mime.h>
#include <h/mhparse.h>

extern int errno;
extern int debugsw;

/*
 * limit actions to specified parts or content types
 */
int npart = 0;
int ntype = 0;
char *parts[NPARTS + 1];
char *types[NTYPES + 1];

int endian = 0;		/* little or big endian */
int userrs = 0;

static char *errs = NULL;


/*
 * prototypes
 */
int part_ok (CT, int);
int type_ok (CT, int);
void set_endian (void);
int make_intermediates (char *);
void content_error (char *, CT, char *, ...);
void flush_errors (void);


int
part_ok (CT ct, int sP)
{
    char **ap;

    if (npart == 0 || (ct->c_type == CT_MULTIPART && (sP || ct->c_subtype)))
	return 1;

    for (ap = parts; *ap; ap++)
	if (!strcmp (*ap, ct->c_partno))
	    return 1;

    return 0;
}


int
type_ok (CT ct, int sP)
{
    char **ap;
    char buffer[BUFSIZ];
    CI ci = &ct->c_ctinfo;

    if (ntype == 0 || (ct->c_type == CT_MULTIPART && (sP || ct->c_subtype)))
	return 1;

    snprintf (buffer, sizeof(buffer), "%s/%s", ci->ci_type, ci->ci_subtype);
    for (ap = types; *ap; ap++)
	if (!strcasecmp (*ap, ci->ci_type) || !strcasecmp (*ap, buffer))
	    return 1;

    return 0;
}


void
set_endian (void)
{
    union {
	long l;
	char c[sizeof(long)];
    } un;

    un.l = 1;
    endian = un.c[0] ? -1 : 1;
    if (debugsw)
	fprintf (stderr, "%s endian architecture\n",
		endian > 0 ? "big" : "little");
}


int
make_intermediates (char *file)
{
    char *cp;

    for (cp = file + 1; (cp = strchr(cp, '/')); cp++) {
	struct stat st;

	*cp = '\0';
	if (stat (file, &st) == NOTOK) {
	    int answer;
	    char *ep;
	    if (errno != ENOENT) {
		advise (file, "error on directory");
losing_directory:
		*cp = '/';
		return NOTOK;
	    }

	    ep = concat ("Create directory \"", file, "\"? ", NULL);
	    answer = getanswer (ep);
	    free (ep);

	    if (!answer)
		goto losing_directory;
	    if (!makedir (file)) {
		advise (NULL, "unable to create directory %s", file);
		goto losing_directory;
	    }
	}

	*cp = '/';
    }

    return OK;
}


/*
 * Construct error message for content
 */

void
content_error (char *what, CT ct, char *fmt, ...)
{
    va_list arglist;
    int	i, len, buflen;
    char *bp, buffer[BUFSIZ];
    CI ci;

    bp = buffer;
    buflen = sizeof(buffer);

    if (userrs && invo_name && *invo_name) {
	snprintf (bp, buflen, "%s: ", invo_name);
	len = strlen (bp);
	bp += len;
	buflen -= len;
    }

    va_start (arglist, fmt);

    vsnprintf (bp, buflen, fmt, arglist);
    len = strlen (bp);
    bp += len;
    buflen -= len;

    ci = &ct->c_ctinfo;

    if (what) {
	char *s;

	if (*what) {
	    snprintf (bp, buflen, " %s: ", what);
	    len = strlen (bp);
	    bp += len;
	    buflen -= len;
	}

	if ((s = strerror (errno)))
	    snprintf (bp, buflen, "%s", s);
	else
	    snprintf (bp, buflen, "Error %d", errno);

	len = strlen (bp);
	bp += len;
	buflen -= len;
    }

    i = strlen (invo_name) + 2;

    /* Now add content type and subtype */
    snprintf (bp, buflen, "\n%*.*s(content %s/%s", i, i, "",
	ci->ci_type, ci->ci_subtype);
    len = strlen (bp);
    bp += len;
    buflen -= len;

    /* Now add the message/part number */
    if (ct->c_file) {
	snprintf (bp, buflen, " in message %s", ct->c_file);
	len = strlen (bp);
	bp += len;
	buflen -= len;

	if (ct->c_partno) {
	    snprintf (bp, buflen, ", part %s", ct->c_partno);
	    len = strlen (bp);
	    bp += len;
	    buflen -= len;
	}
    }

    snprintf (bp, buflen, ")");
    len = strlen (bp);
    bp += len;
    buflen -= len;

    if (userrs) {
	*bp++ = '\n';
	*bp = '\0';
	buflen--;

	errs = add (buffer, errs);
    } else {
	advise (NULL, "%s", buffer);
    }
}


void
flush_errors (void)
{
    if (errs) {
	fflush (stdout);
	fprintf (stderr, "%s", errs);
	free (errs);
	errs = NULL;
    }
}
