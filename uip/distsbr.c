
/*
 * distsbr.c -- routines to do additional "dist-style" processing
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/utils.h>

static int  hdrfd = NOTOK;
static int  txtfd = NOTOK;

#define	BADHDR	"please re-edit %s to remove the ``%s'' header!"
#define	BADTXT	"please re-edit %s to consist of headers only!"
#define	BADMSG	"please re-edit %s to include a ``Resent-To:''!"
#define	BADRFT	"please re-edit %s and fix that header!"

/*
 * static prototypes
 */
static void ready_msg(char *);

int
distout (char *drft, char *msgnam, char *backup)
{
    int state;
    register unsigned char *dp;
    register char *resent;
    char name[NAMESZ], buffer[BUFSIZ];
    register FILE *ifp, *ofp;

    if (rename (drft, strcpy (backup, m_backup (drft))) == NOTOK)
	adios (backup, "unable to rename %s to",drft);
    if ((ifp = fopen (backup, "r")) == NULL)
	adios (backup, "unable to read");

    if ((ofp = fopen (drft, "w")) == NULL)
	adios (drft, "unable to create temporary file");
    chmod (drft, m_gmprot ());

    ready_msg (msgnam);
    lseek (hdrfd, (off_t) 0, SEEK_SET); /* msgnam not accurate */
    cpydata (hdrfd, fileno (ofp), msgnam, drft);

    for (state = FLD, resent = NULL;;)
	switch (state =
		m_getfld (state, name, buffer, sizeof buffer, ifp)) {
	    case FLD: 
	    case FLDPLUS: 
	    case FLDEOF: 
		if (uprf (name, "distribute-"))
		    snprintf (name, sizeof(name), "%s%s", "Resent", &name[10]);
		if (uprf (name, "distribution-"))
		    snprintf (name, sizeof(name), "%s%s", "Resent", &name[12]);
		if (!uprf (name, "resent")) {
		    advise (NULL, BADHDR, "draft", name);
		    goto leave_bad;
		}
		if (state == FLD)
		    resent = add (":", add (name, resent));
		resent = add (buffer, resent);
		fprintf (ofp, "%s: %s", name, buffer);
		while (state == FLDPLUS) {
		    state = m_getfld (state, name,
			    buffer, sizeof buffer, ifp);
		    resent = add (buffer, resent);
		    fputs (buffer, ofp);
		}
		if (state == FLDEOF)
		    goto process;
		break;

	    case BODY: 
	    case BODYEOF: 
		for (dp = buffer; *dp; dp++)
		    if (!isspace (*dp)) {
			advise (NULL, BADTXT, "draft");
			goto leave_bad;
		    }

	    case FILEEOF: 
		goto process;

	    case LENERR: 
	    case FMTERR: 
		advise (NULL, BADRFT, "draft");
	leave_bad: ;
		fclose (ifp);
		fclose (ofp);
		unlink (drft);
		if (rename (backup, drft) == NOTOK)
		    adios (drft, "unable to rename %s to", backup);
		return NOTOK;

	    default: 
		adios (NULL, "getfld() returned %d", state);
	}
process: ;
    fclose (ifp);
    fflush (ofp);

    if (!resent) {
	advise (NULL, BADMSG, "draft");
	fclose (ofp);
	unlink (drft);
	if (rename (backup, drft) == NOTOK)
	    adios (drft, "unable to rename %s to", backup);
	return NOTOK;
    }
    free (resent);

    if (txtfd != NOTOK) {
	lseek (txtfd, (off_t) 0, SEEK_SET); /* msgnam not accurate */
	cpydata (txtfd, fileno (ofp), msgnam, drft);
    }

    fclose (ofp);

    return OK;
}


static void
ready_msg (char *msgnam)
{
    int state, out;
    char name[NAMESZ], buffer[BUFSIZ], tmpfil[BUFSIZ];
    register FILE *ifp, *ofp;

    if (hdrfd != NOTOK)
	close (hdrfd), hdrfd = NOTOK;
    if (txtfd != NOTOK)
	close (txtfd), txtfd = NOTOK;

    if ((ifp = fopen (msgnam, "r")) == NULL)
	adios (msgnam, "unable to open message");

    strncpy (tmpfil, m_tmpfil ("dist"), sizeof(tmpfil));
    if ((hdrfd = open (tmpfil, O_RDWR | O_CREAT | O_TRUNC, 0600)) == NOTOK)
	adios (tmpfil, "unable to re-open temporary file");
    if ((out = dup (hdrfd)) == NOTOK
	    || (ofp = fdopen (out, "w")) == NULL)
	adios (NULL, "no file descriptors -- you lose big");
    unlink (tmpfil);

    for (state = FLD;;)
	switch (state =
		m_getfld (state, name, buffer, sizeof buffer, ifp)) {
	    case FLD: 
	    case FLDPLUS: 
	    case FLDEOF: 
		if (uprf (name, "resent"))
		    fprintf (ofp, "Prev-");
		fprintf (ofp, "%s: %s", name, buffer);
		while (state == FLDPLUS) {
		    state = m_getfld (state, name,
			    buffer, sizeof buffer, ifp);
		    fputs (buffer, ofp);
		}
		if (state == FLDEOF)
		    goto process;
		break;

	    case BODY: 
	    case BODYEOF: 
		fclose (ofp);

		strncpy (tmpfil, m_tmpfil ("dist"), sizeof(tmpfil));
		if ((txtfd = open (tmpfil, O_RDWR | O_CREAT | O_TRUNC, 0600)) == NOTOK)
		    adios (tmpfil, "unable to open temporary file");
		if ((out = dup (txtfd)) == NOTOK
			|| (ofp = fdopen (out, "w")) == NULL)
		    adios (NULL, "no file descriptors -- you lose big");
		unlink (tmpfil);
		fprintf (ofp, "\n%s", buffer);
		while (state == BODY) {
		    state = m_getfld (state, name,
			    buffer, sizeof buffer, ifp);
		    fputs (buffer, ofp);
		}
	    case FILEEOF: 
		goto process;

	    case LENERR: 
	    case FMTERR: 
		adios (NULL, "format error in message %s", msgnam);

	    default: 
		adios (NULL, "getfld() returned %d", state);
	}
process: ;
    fclose (ifp);
    fclose (ofp);
}
