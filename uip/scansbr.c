/* scansbr.c -- routines to help scan along...
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/addrsbr.h>
#include <h/fmt_scan.h>
#include <h/scansbr.h>
#include <h/tws.h>
#include <h/utils.h>

static struct format *fmt;
static struct comp *datecomp;		/* pntr to "date" comp             */
static struct comp *bodycomp;		/* pntr to "body" pseudo-comp      *
					 * (if referenced)                 */
static int ncomps = 0;			/* # of interesting components     */
static char **compbuffers = 0; 		/* buffers for component text      */
static struct comp **used_buf = 0;	/* stack for comp that use buffers */

static int dat[5];			/* aux. data for format routine    */

static m_getfld_state_t gstate;		/* for accessor functions below    */

#define DIEWRERR() adios (scnmsg, "write error on")

#define FPUTS(buf) {\
		if (fputs(buf,scnout) == EOF)\
		    DIEWRERR();\
		}

/* outnum determines how the input from inb is copied.  If positive then
 * it is the number of the message to create, e.g. for inc(1), and all
 * of the email is copied into that message, with some tweaks.  If 0,
 * e.g. `scan 42', then reading inb can dubiously stop after a whole
 * buffer of body, even though this might not be enough to fulfill the
 * scan format and width.  Or if -1 then no copy is being created, but
 * all of inb must be read because the next message must be found, e.g.
 * `scan -file foo.mbox'. */

int
scan (FILE *inb, int innum, int outnum, char *nfs, int width, int curflg,
      int unseen, char *folder, long size, int noisy, charstring_t *scanl)
{
    int i, compnum, encrypted, state;
    char *cp, *tmpbuf, *startbody, **nxtbuf;
    char *saved_c_text = NULL;
    struct comp *cptr;
    struct comp **savecomp;
    char *scnmsg = NULL;
    FILE *scnout = NULL;
    char name[NAMESZ];
    int bufsz;
    static int rlwidth, slwidth;

    /* first-time only initialization, which will always happen the
       way the code is now, with callers initializing *scanl to NULL.
       scanl used to be a global. */
    if (! *scanl) {
	if (width == -1) {
	    /* Default:  width of the terminal, but at least WIDTH/2. */
	    if ((width = sc_width ()) < WIDTH/2)
		width = WIDTH/2;
	} else if (width == 0) {
	    /* Unlimited width. */
	    width = INT_MAX;
	}
	dat[3] = slwidth = width;
        *scanl = charstring_create (min(width, NMH_BUFSIZ));
	if (outnum)
	    umask(~m_gmprot());

	/* Compile format string */
	ncomps = fmt_compile (nfs, &fmt, 1) + 2;

	bodycomp = fmt_findcomp("body");
	datecomp = fmt_findcomp("date");
	cptr = fmt_findcomp("folder");
	if (cptr && folder)
	    cptr->c_text = mh_xstrdup(folder);
	if (fmt_addcompentry("encrypted")) {
		ncomps++;
	}
	cptr =  fmt_findcomp("dtimenow");
	if (cptr)
	    cptr->c_text = getcpy(dtimenow (0));

	/*
	 * In other programs I got rid of this complicated buffer switching,
	 * but since scan reads lots of messages at once and this complicated
	 * memory management, I decided to keep it; otherwise there was
	 * the potential for a lot of malloc() and free()s, and I could
	 * see the malloc() pool really getting fragmented.  Maybe it
	 * wouldn't be an issue in practice; perhaps this will get
	 * revisited someday.
	 *
	 * So, some notes for what's going on:
	 *
	 * nxtbuf is an array of pointers that contains malloc()'d buffers
	 * to hold our component text.  used_buf is an array of struct comp
	 * pointers that holds pointers to component structures we found while
	 * processing a message.
	 *
	 * We read in the message with m_getfld(), using "tmpbuf" as our
	 * input buffer.  tmpbuf is set at the start of message processing
	 * to the first buffer in our buffer pool (nxtbuf).
	 *
	 * Every time we find a component we care about, we set that component's
	 * text buffer to the current value of tmpbuf, and then switch tmpbuf
	 * to the next buffer in our pool.  We also add that component to
	 * our used_buf pool.
	 *
	 * When we're done, we go back and zero out all of the component
	 * text buffer pointers that we saved in used_buf.
	 *
	 * Note that this means c_text memory is NOT owned by the fmt_module
	 * and it's our responsibility to free it.
	 */

	nxtbuf = compbuffers = mh_xcalloc(ncomps, sizeof *nxtbuf);
	used_buf = mh_xcalloc(ncomps + 1, sizeof *used_buf);
	used_buf += ncomps+1; *--used_buf = 0;
	rlwidth = NMH_BUFSIZ;
	for (i = ncomps; i--; )
	    *nxtbuf++ = mh_xmalloc(rlwidth);
    }

    /*
     * each-message initialization
     */
    nxtbuf = compbuffers;
    savecomp = used_buf;
    tmpbuf = *nxtbuf++;
    startbody = NULL;
    dat[0] = innum ? innum : outnum;
    dat[1] = curflg;
    dat[4] = unseen;

    /*
     * Get the first field.  If the message is non-empty
     * and we're doing an "inc", open the output file.
     */
    bufsz = rlwidth;
    m_getfld_state_reset (&gstate);
    if ((state = m_getfld (&gstate, name, tmpbuf, &bufsz, inb)) == FILEEOF) {
	if (ferror(inb)) {
	    advise("read", "unable to"); /* "read error" */
	    return SCNFAT;
	}
        return SCNEOF;
    }

    if (outnum > 0) {
        scnmsg = m_name (outnum);
        if (*scnmsg == '?')		/* msg num out of range */
            return SCNNUM;
        if ((scnout = fopen (scnmsg, "w")) == NULL)
            adios (scnmsg, "unable to write");
    }

    /* scan - main loop */
    for (compnum = 1; ;
	bufsz = rlwidth, state = m_getfld (&gstate, name, tmpbuf, &bufsz, inb)) {
	switch (state) {
	    case FLD: 
	    case FLDPLUS: 
		compnum++;
		if (scnout) {
		    FPUTS (name);
		    if ( putc (':', scnout) == EOF) DIEWRERR();
		    FPUTS (tmpbuf);
		}
		/*
		 * if we're interested in this component, save a pointer
		 * to the component text, then start using our next free
		 * buffer as the component temp buffer (buffer switching
		 * saves an extra copy of the component text).
		 */
		if ((cptr = fmt_findcasecomp(name))) {
		    if (! cptr->c_text) {
			cptr->c_text = tmpbuf;
			for (cp = tmpbuf + strlen (tmpbuf) - 1; 
					cp >= tmpbuf; cp--)
			    if (isspace ((unsigned char) *cp))
				*cp = 0;
			    else
				break;
			*--savecomp = cptr;
			tmpbuf = *nxtbuf++;
		    }
		}

		while (state == FLDPLUS) {
		    bufsz = rlwidth;
		    state = m_getfld (&gstate, name, tmpbuf, &bufsz, inb);
		    if (scnout)
			FPUTS (tmpbuf);
		}
		break;

	    case BODY: 
		/*
		 * A slight hack ... if we have less than rlwidth characters
		 * in the buffer, call m_getfld again.
		 */

		if ((i = strlen(tmpbuf)) < rlwidth) {
		    bufsz = rlwidth - i;
		    state = m_getfld (&gstate, name, tmpbuf + i, &bufsz, inb);
		}

		if (outnum == 0) {
		    state = FILEEOF; /* stop now if scan cmd */
		    if (bodycomp && startbody == NULL)
		    	startbody = tmpbuf;
		    goto finished;
		}
                if (scnout) {
                    if (putc ('\n', scnout) == EOF) DIEWRERR();
                    FPUTS (tmpbuf);
                }
		/*
                 * The previous code here used to call m_getfld() using
                 * pointers to the underlying output stdio buffers to
                 * avoid the extra copy.  Tests by Markus Schnalke show
                 * no noticeable performance loss on larger mailboxes
                 * if we incur an extra copy, and messing around with
                 * internal stdio buffers is becoming more and more
                 * unportable as times go on.  So from now on just deal
                 * with the overhead of an extra copy.
		 *
		 * Subtle change - with the previous code tmpbuf wasn't
		 * used, so we could reuse it for the {body} component.
		 * Now since we're using tmpbuf as our read buffer we
		 * need to save the beginning of the body for later.
		 * See the above (and below) use of startbody.
		 */
body:;
		if (bodycomp && startbody == NULL) {
		    startbody = tmpbuf;
		    tmpbuf = *nxtbuf++;
		}

		while (state == BODY) {
		    bufsz = rlwidth;
		    state = m_getfld (&gstate, name, tmpbuf, &bufsz, inb);
                    if (scnout)
                        FPUTS(tmpbuf);
		}
		goto finished;

	    case LENERR: 
	    case FMTERR: 
	    	if (innum)
		    fprintf (stderr, "??Format error (message %d) in ",
			     outnum ? outnum : innum);
		else
		    fprintf (stderr, "??Format error in ");

		fprintf (stderr, "component %d\n", compnum);

		if (scnout) {
		    FPUTS ("\n\nBAD MSG:\n");
		    FPUTS (name);
		    if (putc ('\n', scnout) == EOF) DIEWRERR();
		    state = BODY;
		    goto body;
		}
		goto finished;

	    case FILEEOF:
		goto finished;

	    default: 
		adios (NULL, "getfld() returned %d", state);
	}
    }

    /*
     * format and output the scan line.
     */
finished:
    if (ferror(inb)) {
	advise("read", "unable to"); /* "read error" */
	return SCNFAT;
    }

    /* Save and restore buffer so we don't trash our dynamic pool! */
    if (bodycomp) {
	saved_c_text = bodycomp->c_text;
	bodycomp->c_text = startbody;
    }

    if (size)
	dat[2] = size;
    else if (scnout)
    {
	dat[2] = ftell(scnout);
	if (dat[2] == EOF) DIEWRERR();
    }

    if ((datecomp && !datecomp->c_text) || (!size && !outnum)) {
	struct stat st;

	fstat (fileno(inb), &st);
	if (!size && !outnum)
	    dat[2] = st.st_size;
	if (datecomp) {
	    if (! datecomp->c_text) {
		if (datecomp->c_tws == NULL)
		    NEW0(datecomp->c_tws);
		*datecomp->c_tws = *dlocaltime ((time_t *) &st.st_mtime);
		datecomp->c_flags |= CF_DATEFAB|CF_TRUE;
	    } else {
		datecomp->c_flags &= ~CF_DATEFAB;
	    }
	}
    }

    fmt_scan (fmt, *scanl, slwidth, dat, NULL);

    if (bodycomp)
	bodycomp->c_text = saved_c_text;

    if (noisy)
	fputs (charstring_buffer (*scanl), stdout);

    cptr = fmt_findcomp ("encrypted");
    encrypted = cptr && cptr->c_text;

    /* return dynamically allocated buffers to pool */
    while ((cptr = *savecomp++)) {
	cptr->c_text = NULL;
    }

    if (scnout && (ferror(scnout) || fclose (scnout) == EOF))
	DIEWRERR();

    return (state != FILEEOF ? SCNERR : encrypted ? SCNENC : SCNMSG);
}


/* The following two functions allow access to the global gstate above. */
void
scan_finished(void) {
    m_getfld_state_destroy (&gstate);
}

void
scan_detect_mbox_style (FILE *f) {
    m_unknown (&gstate, f);
}
