/* replsbr.c -- routines to help repl along...
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/fmt_new.h"
#include "sbr/m_name.h"
#include "sbr/m_gmprot.h"
#include "sbr/m_getfld.h"
#include "sbr/read_switch.h"
#include "sbr/concat.h"
#include "sbr/uprf.h"
#include "sbr/escape_addresses.h"
#include "sbr/pidstatus.h"
#include "sbr/arglist.h"
#include "sbr/error.h"
#include "h/addrsbr.h"
#include "h/fmt_scan.h"
#include "h/done.h"
#include "h/utils.h"
#include <sys/file.h>
#include "replsbr.h"

short ccto = -1;
short cccc = -1;
short ccme = -1;
short querysw = 0;

static int dftype=0;

static char *badaddrs = NULL;
static char *dfhost = NULL;

static struct mailname mq;
static bool nodupcheck;		/* If set, no check for duplicates */

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

/*
 * static prototypes
 */
static int insert (struct mailname *);
static void replfilter (FILE *, FILE *, char *, int);
static char *replformataddr(char *, char *);
static char *replconcataddr(char *, char *);
static char *fix_addresses (char *);


void
replout (FILE *inb, char *msg, char *drft, struct msgs *mp, int outputlinelen,
	int mime, char *form, char *filter, char *fcc, int fmtproc)
{
    int state, i;
    struct comp *cptr;
    char tmpbuf[NMH_BUFSIZ];
    struct format *fmt;
    char **ap;
    int	char_read = 0, format_len, mask;
    char name[NAMESZ], *cp;
    charstring_t scanl;
    static int dat[5];			/* aux. data for format routine */
    m_getfld_state_t gstate;
    struct fmt_callbacks cb;

    FILE *out;
    NMH_UNUSED (msg);

    mask = umask(~m_gmprot());
    if ((out = fopen (drft, "w")) == NULL)
	adios (drft, "unable to create");

    umask(mask);

    /* get new format string */
    cp = new_fs (form, NULL, NULL);
    format_len = strlen (cp);

    /* compile format string */
    fmt_compile (cp, &fmt, 1);

    for (ap = addrcomps; *ap; ap++) {
	cptr = fmt_findcomp (*ap);
	if (cptr)
	    cptr->c_type |= CT_ADDR;
    }

    /*
     * ignore any components killed by command line switches
     *
     * This prevents the component from being found via fmt_findcomp(),
     * which makes sure no text gets added to it when the message is processed.
     */
    if (!ccto) {
	cptr = fmt_findcomp ("to");
	if (cptr)
	    cptr->c_name = mh_xstrdup("");
    }
    if (!cccc) {
	 cptr = fmt_findcomp("cc");
	if (cptr)
	    cptr->c_name = mh_xstrdup("");
    }
    if (!ccme)
	ismymbox (NULL);

    /*
     * pick any interesting stuff out of msg "inb"
     */
    gstate = m_getfld_state_init(inb);
    for (;;) {
	int msg_count = sizeof tmpbuf;
	state = m_getfld2(&gstate, name, tmpbuf, &msg_count);
	switch (state) {
	    case FLD:
	    case FLDPLUS:
		/*
		 * if we're interested in this component, save a pointer
		 * to the component text, then start using our next free
		 * buffer as the component temp buffer (buffer switching
		 * saves an extra copy of the component text).
		 */

		i = fmt_addcomptext(name, tmpbuf);
		if (i != -1) {
		    char_read += msg_count;
		    while (state == FLDPLUS) {
			msg_count= sizeof tmpbuf;
			state = m_getfld2(&gstate, name, tmpbuf, &msg_count);
			fmt_appendcomp(i, name, tmpbuf);
			char_read += msg_count;
		    }
		}

		while (state == FLDPLUS) {
		    msg_count= sizeof tmpbuf;
		    state = m_getfld2(&gstate, name, tmpbuf, &msg_count);
		}
		break;

	    case LENERR:
	    case FMTERR:
	    case BODY:
	    case FILEEOF:
		goto finished;

	    default:
		die("m_getfld2() returned %d", state);
	}
    }

    /*
     * format and output the header lines.
     */
finished:
    m_getfld_state_destroy (&gstate);

    /* set up the "fcc" pseudo-component */
    cptr = fmt_findcomp ("fcc");
    if (cptr) {
	free(cptr->c_text);
	if (fcc)
	    cptr->c_text = mh_xstrdup(fcc);
	else
	    cptr->c_text = NULL;
    }
    cptr = fmt_findcomp ("user");
    if (cptr) {
	free(cptr->c_text);
	if ((cp = getenv("USER")))
	    cptr->c_text = mh_xstrdup(cp);
	else
	    cptr = NULL;
    }

    /*
     * if there's a "Subject" component, strip any "Re:"s off it
     */
    cptr = fmt_findcomp ("subject");
    if (cptr && (cp = cptr->c_text)) {
	char *sp = cp;

	for (;;) {
	    while (isspace((unsigned char) *cp))
		cp++;
	    if(uprf(cp, "re:"))
		cp += 3;
	    else
		break;
	    sp = cp;
	}
	if (sp != cptr->c_text) {
	    cp = cptr->c_text;
	    cptr->c_text = mh_xstrdup(sp);
	    free (cp);
	}
    }
    i = format_len + char_read + 256;
    scanl = charstring_create (i + 2);
    dat[0] = 0;
    dat[1] = 0;
    dat[2] = 0;
    dat[3] = outputlinelen;
    dat[4] = 0;
    ZERO(&cb);
    cb.formataddr = replformataddr;
    cb.concataddr = replconcataddr;
    fmt_scan (fmt, scanl, i, dat, &cb);
    fputs (charstring_buffer (scanl), out);
    if (badaddrs) {
	fputs ("\nrepl: bad addresses:\n", out);
	fputs ( badaddrs, out);
    }

    /*
     * Check if we should filter the message
     * or add mhn directives
     */
    if (filter) {
	fflush(out);
	if (ferror (out))
	    adios (drft, "error writing");
	
	replfilter (inb, out, filter, fmtproc);
    } else if (mime && mp) {
	    fprintf (out, "#forw [original message] +%s %s\n",
		     mp->foldpath, m_name (mp->lowsel));
    }

    fflush(out);
    if (ferror (out))
	adios (drft, "error writing");
    fclose (out);

    /* return dynamically allocated buffers */
    charstring_free (scanl);
    fmt_free(fmt, 1);
}

static char *buf;		/* our current working buffer */
static char *bufend;		/* end of working buffer */
static char *last_dst;		/* buf ptr at end of last call */
static unsigned int bufsiz=0;	/* current size of buf */

#define BUFINCR 512		/* how much to expand buf when if fills */

#define CPY(s) { cp = (s); while ((*dst++ = *cp++)) ; --dst; }

/*
 * check if there's enough room in buf for str.
 * add more mem if needed
 */
#define CHECKMEM(str) \
	    if ((len = strlen (str)) >= bufend - dst) {\
		int i = dst - buf;\
		int n = last_dst - buf;\
		bufsiz += ((dst + len - bufend) / BUFINCR + 1) * BUFINCR;\
		buf = mh_xrealloc (buf, bufsiz);\
		dst = buf + i;\
		last_dst = buf + n;\
		bufend = buf + bufsiz;\
	    }


/*
 * fmt_scan will call this routine if the user includes the function
 * "(formataddr {component})" in a format string.  "orig" is the
 * original contents of the string register.  "str" is the address
 * string to be formatted and concatenated onto orig.  This routine
 * returns a pointer to the concatenated address string.
 *
 * We try to not do a lot of malloc/copy/free's (which is why we
 * don't call "getcpy") but still place no upper limit on the
 * length of the result string.
 */
static char *
replformataddr (char *orig, char *str)
{
    int len;
    char baddr[BUFSIZ+6], error[BUFSIZ];
    bool isgroup;
    char *dst;
    char *cp;
    char *sp;
    struct mailname *mp = NULL;
    char *fixed_str = fix_addresses (str);

    /* if we don't have a buffer yet, get one */
    if (bufsiz == 0) {
	buf = mh_xmalloc (BUFINCR);
	last_dst = buf;		/* XXX */
	bufsiz = BUFINCR - 6;  /* leave some slop */
	bufend = buf + bufsiz;
    }
    /*
     * If "orig" points to our buffer we can just pick up where we
     * left off.  Otherwise we have to copy orig into our buffer.
     */
    if (orig == buf)
	dst = last_dst;
    else if (!orig || !*orig) {
	dst = buf;
	*dst = '\0';
    } else {
	dst = last_dst;		/* XXX */
	CHECKMEM (orig);
	CPY (orig);
    }

    /* concatenate all the new addresses onto 'buf' */
    for (isgroup = false; (cp = getname (fixed_str)); ) {
	if ((mp = getm (cp, dfhost, dftype, error, sizeof(error))) == NULL) {
	    snprintf (baddr, sizeof(baddr), "\t%s -- %s\n", cp, error);
	    badaddrs = add (baddr, badaddrs);
	    continue;
	}
	if (isgroup && (mp->m_gname || !mp->m_ingrp)) {
	    *dst++ = ';';
	    isgroup = false;
	}
	if (insert (mp)) {
	    /* if we get here we're going to add an address */
	    if (dst != buf) {
		*dst++ = ',';
		*dst++ = ' ';
	    }
	    if (mp->m_gname) {
		CHECKMEM (mp->m_gname);
		CPY (mp->m_gname);
		isgroup = true;
	    }
	    sp = adrformat (mp);
	    CHECKMEM (sp);
	    CPY (sp);
	}
    }

    free (fixed_str);

    if (isgroup)
	*dst++ = ';';

    *dst = '\0';
    last_dst = dst;
    return buf;
}


/*
 * fmt_scan will call this routine if the user includes the function
 * "(concataddr {component})" in a format string.  This behaves exactly
 * like formataddr, except that it does NOT suppress duplicate addresses
 * between calls.
 *
 * As an implementation detail: I thought about splitting out replformataddr()
 * into the generic part and duplicate-suppressing part, but the call to
 * insert() was buried deep within a couple of loops and I didn't see a
 * way to do it easily.  So instead we simply set a special flag to stop
 * the duplicate check and call replformataddr().
 */
static char *
replconcataddr(char *orig, char *str)
{
    char *cp;

    nodupcheck = true;
    cp = replformataddr(orig, str);
    nodupcheck = false;
    return cp;
}

static int
insert (struct mailname *np)
{
    char buffer[BUFSIZ];
    struct mailname *mp;

    if (nodupcheck)
	return 1;

    if (np->m_mbox == NULL)
	return 0;

    for (mp = &mq; mp->m_next; mp = mp->m_next) {
	if (!strcasecmp (FENDNULL(np->m_host),
			 FENDNULL(mp->m_next->m_host))  &&
	    !strcasecmp (FENDNULL(np->m_mbox),
			 FENDNULL(mp->m_next->m_mbox)))
	    return 0;
    }
    if (!ccme && ismymbox (np))
	return 0;

    if (querysw) {
	snprintf (buffer, sizeof(buffer), "Reply to %s? ", adrformat (np));
	if (!read_switch (buffer, anoyes))
	return 0;
    }
    mp->m_next = np;

    return 1;
}


/*
 * Call the mhlproc
 *
 * This function expects that argument out has been fflushed by the caller.
 */

static void
replfilter (FILE *in, FILE *out, char *filter, int fmtproc)
{
    int	pid;
    char *mhl;
    char *errstr;
    char **arglist;
    int argnum;

    if (filter == NULL)
	return;

    if (access (filter, R_OK) == NOTOK)
	adios (filter, "unable to read");

    rewind (in);
    lseek(fileno(in), 0, SEEK_SET);

    arglist = argsplit(mhlproc, &mhl, &argnum);

    switch (pid = fork()) {
	case NOTOK:
	    adios ("fork", "unable to");

	case OK:
	    dup2 (fileno (in), fileno (stdin));
	    dup2 (fileno (out), fileno (stdout));

	    /*
	     * We're not allocating the memory for the extra arguments,
	     * because we never call arglist_free().  But if we ever change
	     * that be sure to use getcpy() for the extra arguments.
	     */
	    arglist[argnum++] = "-form";
	    arglist[argnum++] = filter;
	    arglist[argnum++] = "-noclear";

	    switch (fmtproc) {
	    case 1:
		arglist[argnum++] = "-fmtproc";
		arglist[argnum++] = formatproc;
		break;
	    case 0:
		arglist[argnum++] = "-nofmtproc";
		break;
	    }

	    arglist[argnum++] = NULL;

	    execvp (mhl, arglist);
	    errstr = strerror(errno);
	    if (write(2, "unable to exec ", 15) < 0  ||
		write(2, mhlproc, strlen(mhlproc)) < 0  ||
		write(2, ": ", 2) < 0  ||
		write(2, errstr, strlen(errstr)) < 0  ||
		write(2, "\n", 1) < 0) {
		advise ("stderr", "write");
	    }
	    _exit(1);

	default:
	    if (pidXwait (pid, mhl))
		done (1);
	    fseek (out, 0L, SEEK_END);
            arglist_free(mhl, arglist);
	    break;
    }
}


static char *
fix_addresses (char *str)
{
    char *fixed_str = NULL;
    bool fixed_address = false;

    if (str) {
        /*
         * Attempt to parse each of the addresses in str.  If any fail
         * and can be fixed with escape_local_part(), do that.  This
         * is extra ugly because getm()'s state can only be reset by
         * call getname(), and getname() needs to be called repeatedly
         * until it returns NULL to reset its state.
         */
        struct adr_node {
            char *adr;
            int escape_local_part;
            int fixed;
            struct adr_node *next;
        } *adrs = NULL;
        struct adr_node *np = adrs;
        char *cp;

        /*
         * First, put each of the addresses in a linked list.  Note
         * invalid addresses that might be fixed by escaping the
         * local part.
         */
        while ((cp = getname (str))) {
            struct adr_node *adr_nodep;
            char error[BUFSIZ];
            struct mailname *mp;

            NEW(adr_nodep);
            adr_nodep->adr = mh_xstrdup (cp);
            adr_nodep->escape_local_part = 0;
            adr_nodep->fixed = 0;
            adr_nodep->next = NULL;

            /* With AD_NAME, errors are not reported to user. */
            if ((mp = getm (cp, dfhost, dftype, error,
			    sizeof(error))) == NULL) {
                const char *no_at_sign = "no at-sign after local-part";

                adr_nodep->escape_local_part =
                    has_prefix(error, no_at_sign);
            } else {
                mnfree (mp);
            }

            if (np) {
                np = np->next = adr_nodep;
            } else {
                np = adrs = adr_nodep;
            }
        }

        /*
         * Walk the list and try to fix broken addresses.
         */
        for (np = adrs; np; np = np->next) {
            char *display_name = mh_xstrdup (np->adr);
            size_t len = strlen (display_name);

            if (np->escape_local_part) {
                char *local_part_end = strrchr (display_name, '<');
                char *angle_addr = mh_xstrdup (local_part_end);
                struct mailname *mp;
                char *new_adr, *adr;

                *local_part_end = '\0';
                /* Trim any trailing whitespace. */
                while (local_part_end > display_name  &&
                       isspace ((unsigned char) *--local_part_end)) {
                    *local_part_end = '\0';
                }
                escape_local_part (display_name, len);
                new_adr = concat (display_name, " ", angle_addr, NULL);
                adr = getname (new_adr);
                if (adr != NULL  &&
                    (mp = getm (adr, dfhost, dftype, NULL, 0)) != NULL) {
                    fixed_address = true;
                    mnfree (mp);
                }
                free (angle_addr);
                free (new_adr);
                free (np->adr);
                np->adr = mh_xstrdup (adr);

                /* Need to flush getname() */
                while ((cp = getname (""))) continue;
            } /* else the np->adr is OK, so use it as-is. */

            free (display_name);
        }

        /*
         * If any addresses were repaired, build new address string,
         * replacing broken addresses.
         */
        for (np = adrs; np; ) {
            struct adr_node *next = np->next;

            if (fixed_address) {
                if (fixed_str) {
                    char *new_str = concat (fixed_str, ", ", np->adr, NULL);

                    free (fixed_str);
                    fixed_str = new_str;
                } else {
                    fixed_str = mh_xstrdup (np->adr);
                }
            }

            free (np->adr);
            free (np);
            np = next;
        }
    }

    if (fixed_address) {
        return fixed_str;
    }
    free (fixed_str);
    return str  ?  mh_xstrdup (str)  :  NULL;
}
