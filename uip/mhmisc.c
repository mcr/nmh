/* mhmisc.c -- misc routines to process MIME messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/mime.h>
#include <h/mhparse.h>
#include <h/utils.h>
#include "../sbr/makedir.h"

extern int debugsw;

/*
 * limit actions to specified parts or content types
 */
int npart = 0;
int ntype = 0;
char *parts[NPARTS + 1];
char *types[NTYPES + 1];

int userrs = 0;

static char *errs = NULL;


/*
 * prototypes
 */
int part_ok (CT);
int part_exact(CT ct);
int type_ok (CT, int);
void content_error (char *, CT, char *, ...) CHECK_PRINTF(3, 4);
void flush_errors (void);


int
part_ok (CT ct)
{
    char **ap;
    int len;

    /* a part is "ok", i.e., should be processed, if:
	- there were no -part arguments
	- this part is a multipart
     */
    if (npart == 0 || ct->c_type == CT_MULTIPART) {
	return 1;
    }

    /* or if:
	- this part is a an exact match for any -part option
	- this part is a sub-part of any -part option
     */
    for (ap = parts; *ap; ap++) {
        len = strlen(*ap);
        if (!strncmp (*ap, ct->c_partno, len) &&
                (!ct->c_partno[len] || ct->c_partno[len] == '.' )) {
            return 1;
	}
    }

    return 0;
}

int
part_exact(CT ct)
{
    char **ap;

    if (!ct->c_partno)
	return 0;

    for (ap = parts; *ap; ap++) {
        if (!strcmp (*ap, ct->c_partno)) {
            return 1;
	}
    }

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


/*
 * Returns true if this content is marked as "inline".
 *
 * Technically we should check parent content to see if they have
 * disposition to use as a default, but we don't right now.  Maybe
 * later ....
 */

int
is_inline(CT ct)
{
    /*
     * If there isn't any disposition at all, it's "inline".  Obviously
     * if it's "inline", then it's inline.  RFC 2183 says if it's an unknown
     * disposition, treat it as 'attachment'.
     */

    if (! ct->c_dispo_type || strcasecmp(ct->c_dispo_type, "inline") == 0)
	return 1;
    return 0;
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
	    answer = read_yes_or_no_if_tty (ep);
	    free (ep);

	    if (!answer)
		goto losing_directory;
	    if (!makedir (file)) {
		inform("unable to create directory %s", file);
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
    snprintf (bp, buflen, "\n%*s(content %s/%s", i, "",
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
	inform("%s", buffer);
    }
}


void
flush_errors (void)
{
    if (errs) {
	fflush (stdout);
	fputs(errs, stderr);
	free (errs);
	errs = NULL;
    }
}
