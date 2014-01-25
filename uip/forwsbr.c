
/*
 * forwsbr.c -- subroutine to build a draft from a component file
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/fmt_scan.h>
#include <h/tws.h>
#include <h/utils.h>

/*
 * Take from replsbr.c - a buffer big enough to read in data header lines
 * in reasonable chunks but not enough to slurp in the whole message
 */

static char msgbuf[256];
#define COMPFREE(c) if (c->c_text) free(c->c_text)

/*
 * A list of components we treat as addresses
 */

static char *addrcomps[] = {
    "from",
    "sender",
    "reply-to",
    "to",
    "cc",
    "bcc",
    "resent-from",
    "resent-sender",
    "resent-reply-to",
    "resent-to",
    "resent-cc",
    "resent-bcc",
    NULL
};

int
build_form (char *form, char *digest, int *dat, char *from, char *to,
	    char *cc, char *fcc, char *subject, char *inputfile)
{
    int	in;
    int fmtsize, state;
    int i;
    register char *nfs;
    char *line, tmpfil[BUFSIZ], name[NAMESZ], **ap;
    FILE *tmp;
    register struct comp *cptr;
    struct format *fmt;
    char *cp = NULL;
    m_getfld_state_t gstate = 0;

    /*
     * Open the message we'll be scanning for components
     */

    if ((tmp = fopen(inputfile, "r")) == NULL)
    	adios (inputfile, "Unable to open");

    /* Get new format string */
    nfs = new_fs (form, NULL, NULL);
    fmtsize = strlen (nfs) + 256;

    /* Compile format string */
    (void) fmt_compile (nfs, &fmt, 1);

    /*
     * Mark any components tagged as address components
     */

    for (ap = addrcomps; *ap; ap++) {
	cptr = fmt_findcomp (*ap);
	if (cptr)
	    cptr->c_type |= CT_ADDR;
    }

    /*
     * Process our message and save all relevant components
     *
     * A lot of this is taken from replsbr.c; should we try to merge
     * these routines?
     */

    for (;;) {
	int msg_count = sizeof msgbuf;
	state = m_getfld (&gstate, name, msgbuf, &msg_count, tmp);
	switch (state) {
	    case FLD:
	    case FLDPLUS:
	    	/*
		 * If we find a component that we're interested in, save
		 * a copy.  We don't do all of that weird buffer switching
		 * that replout does.
		 */

		i = fmt_addcomptext(name, msgbuf);
		if (i != -1) {
		    while (state == FLDPLUS) {
			msg_count = sizeof msgbuf;
			state = m_getfld (&gstate, name, msgbuf, &msg_count, tmp);
			fmt_appendcomp(i, name, msgbuf);
		    }
		}
		while (state == FLDPLUS) {
		    msg_count = sizeof msgbuf;
		    state = m_getfld (&gstate, name, msgbuf, &msg_count, tmp);
		}
		break;

	    case LENERR:
	    case FMTERR:
	    case BODY:
	    case FILEEOF:
	    	goto finished;

	    default:
	    	adios(NULL, "m_getfld() returned %d", state);
	}
    }

    /*
     * Override any components just in case they were included in the
     * input message.  Also include command-line components given here
     *
     * With the memory rework I've changed things so we always get copies
     * of these strings; I don't like the idea that the caller of this
     * function has to know to pass in already-allocated memory (and that
     * it will be free()'d by us).
     */

finished:
    m_getfld_state_destroy (&gstate);

    cptr = fmt_findcomp ("digest");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = getcpy(digest);
    }
    cptr = fmt_findcomp ("nmh-date");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = getcpy(dtimenow (0));
    }
    cptr = fmt_findcomp ("nmh-from");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = getcpy(from);
    }
    cptr = fmt_findcomp ("nmh-to");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = getcpy(to);
    }
    cptr = fmt_findcomp ("nmh-cc");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = getcpy(cc);
    }
    cptr = fmt_findcomp ("nmh-subject");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = getcpy(subject);
    }
    cptr = fmt_findcomp ("fcc");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = getcpy(fcc);
    }

    cp = m_mktemp2(NULL, invo_name, NULL, &tmp);
    if (cp == NULL) {
	adios(NULL, "unable to create temporary file in %s", get_temp_dir());
    }
    strncpy (tmpfil, cp, sizeof(tmpfil));
    (void) m_unlink (tmpfil);
    if ((in = dup (fileno (tmp))) == NOTOK)
	adios ("dup", "unable to");

    line = mh_xmalloc ((unsigned) fmtsize);
    fmt_scan (fmt, line, fmtsize - 1, fmtsize, dat, NULL);
    fputs (line, tmp);
    free (line);
    if (fclose (tmp))
	adios (tmpfil, "error writing");

    lseek (in, (off_t) 0, SEEK_SET);

    /*
     * Free any component buffers that we allocated
     */

    fmt_free(fmt, 1);

    return in;
}
