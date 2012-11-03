
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
    int fmtsize, state, char_read = 0;
    unsigned i;
    register char *nfs;
    char *line, tmpfil[BUFSIZ], name[NAMESZ], **ap;
    FILE *tmp;
    register struct comp *cptr;
    struct format *fmt;
    char *cp = NULL;

    /*
     * Open the message we'll be scanning for components
     */

    if ((tmp = fopen(inputfile, "r")) == NULL)
    	adios (inputfile, "Unable to open");

    /* Get new format string */
    nfs = new_fs (form, NULL, NULL);
    fmtsize = strlen (nfs) + 256;

    /* Compile format string */
    (void) fmt_compile (nfs, &fmt);

    /*
     * Mark any components tagged as address components
     */

    for (ap = addrcomps; *ap; ap++) {
	FINDCOMP (cptr, *ap);
	if (cptr)
	    cptr->c_type |= CT_ADDR;
    }

    /*
     * Process our message and save all relevant components
     *
     * A lot of this is taken from replsbr.c; should we try to merge
     * these routines?
     */

    for (state = FLD;;) {
    	state = m_getfld(state, name, msgbuf, sizeof(msgbuf), tmp);
	switch (state) {
	    case FLD:
	    case FLDPLUS:
	    	/*
		 * If we find a component that we're interested in, save
		 * a copy.  We don't do all of that weird buffer switching
		 * that replout does.
		 */
		if ((cptr = wantcomp[CHASH(name)]))
		    do {
		    	if (mh_strcasecmp(name, cptr->c_name) == 0) {
			    char_read += msg_count;
			    if (! cptr->c_text) {
				cptr->c_text = strdup(msgbuf);
			    } else {
				i = strlen(cptr->c_text) - 1;
				if (cptr->c_text[i] == '\n') {
				    if (cptr->c_type & CT_ADDR) {
					cptr->c_text[i] = '\0';
					cptr->c_text = add(",\n\t",
					    		       cptr->c_text);
				    } else {
					cptr->c_text = add ("\t", cptr->c_text);
				    }
				}
				cptr->c_text = add(msgbuf, cptr->c_text);
			    }
			    while (state == FLDPLUS) {
				state = m_getfld(state, name, msgbuf,
						 sizeof(msgbuf), tmp);
				cptr->c_text = add(msgbuf, cptr->c_text);
				char_read += msg_count;
			    }
			    break;
			}
		    } while ((cptr = cptr->c_next));

		while (state == FLDPLUS)
		    state = m_getfld(state, name, msgbuf, sizeof(msgbuf), tmp);
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
     */

finished:

    FINDCOMP (cptr, "digest");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = digest;
    }
    FINDCOMP (cptr, "nmh-date");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = getcpy(dtimenow (0));
    }
    FINDCOMP (cptr, "nmh-from");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = from;
    }
    FINDCOMP (cptr, "nmh-to");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = to;
    }
    FINDCOMP (cptr, "nmh-cc");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = cc;
    }
    FINDCOMP (cptr, "nmh-subject");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = subject;
    }
    FINDCOMP (cptr, "fcc");
    if (cptr) {
    	COMPFREE(cptr);
	cptr->c_text = fcc;
    }

    cp = m_mktemp2(NULL, invo_name, NULL, &tmp);
    if (cp == NULL) adios("forw", "unable to create temporary file");
    strncpy (tmpfil, cp, sizeof(tmpfil));
    unlink (tmpfil);
    if ((in = dup (fileno (tmp))) == NOTOK)
	adios ("dup", "unable to");

    line = mh_xmalloc ((unsigned) fmtsize);
    fmt_scan (fmt, line, fmtsize - 1, fmtsize, dat);
    fputs (line, tmp);
    free (line);
    if (fclose (tmp))
	adios (tmpfil, "error writing");

    lseek (in, (off_t) 0, SEEK_SET);

    /*
     * Free any component buffers that we allocated
     */

    for (i = 0; i < (sizeof(wantcomp) / sizeof(struct comp)); i++)
    	for (cptr = wantcomp[i]; cptr != NULL; cptr = cptr->c_next)
	    COMPFREE(cptr);

    return in;
}
