
/*
 * mhshowsbr.c -- routines to display the contents of MIME messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/signals.h>
#include <h/md5.h>
#include <setjmp.h>
#include <h/mts.h>
#include <h/tws.h>
#include <h/mime.h>
#include <h/mhparse.h>
#include <h/fmt_scan.h>
#include <h/utils.h>
#ifdef HAVE_ICONV
#   include <iconv.h>
#endif /* ! HAVE_ICONV */

extern int debugsw;

int nolist   = 0;

char *progsw = NULL;

/* flags for moreproc/header display */
int nomore   = 0;
char *formsw = NULL;


/* mhmisc.c */
int part_ok (CT, int);
int type_ok (CT, int);
void content_error (char *, CT, char *, ...);
void flush_errors (void);

/*
 * prototypes
 */
int show_content_aux (CT, int, char *, char *, struct format *fmt);

/*
 * static prototypes
 */
static void show_single_message (CT, char *, int, int, int, struct format *);
static void DisplayMsgHeader (CT, char *, int);
static int show_switch (CT, int, int, int, int, struct format *);
static int show_content (CT, int, int, int, struct format *);
static int show_content_aux2 (CT, int, char *, char *, int, int, int, struct format *);
static int show_text (CT, int, int, struct format *);
static int show_multi (CT, int, int, int, int, struct format *);
static int show_multi_internal (CT, int, int, int, int, struct format *);
static int show_multi_aux (CT, int, char *, struct format *);
static int show_message_rfc822 (CT, int, struct format *);
static int show_partial (CT, int);
static int show_external (CT, int, int, int, int, struct format *);
static int parse_display_string (CT, char *, int *, int *, char *, char *,
				 size_t, int multipart);
static int convert_content_charset (CT, char **);
static struct format *compile_marker(char *);
static void output_marker (CT, struct format *, int);
static void free_markercomps (void);
static int pidcheck(int);

/*
 * Components (and list of parameters/components) we care about for the
 * content marker display.
 */

static struct comp *part_comp = NULL;
static struct comp *ctype_comp = NULL;
static struct comp *description_comp = NULL;
static struct comp *dispo_comp = NULL;

struct param_comp_list {
    char *param;
    struct comp *comp;
    struct param_comp_list *next;
};

static struct param_comp_list *ctype_pc_list = NULL;
static struct param_comp_list *dispo_pc_list = NULL;


/*
 * Top level entry point to show/display a group of messages
 */

void
show_all_messages (CT *cts, int concatsw, int textonly, int inlineonly,
		   char *markerform)
{
    CT ct, *ctp;
    struct format *fmt;

    /*
     * If form is not specified, then get default form
     * for showing headers of MIME messages.
     */
    if (!formsw)
	formsw = getcpy (etcpath ("mhl.headers"));

    /*
     * Compile the content marker format line
     */
    fmt = compile_marker(markerform);

    /*
     * If form is "mhl.null", suppress display of header.
     */
    if (!strcmp (formsw, "mhl.null"))
	formsw = NULL;

    for (ctp = cts; *ctp; ctp++) {
	ct = *ctp;

	/* if top-level type is ok, then display message */
	if (type_ok (ct, 1))
	    show_single_message (ct, formsw, concatsw, textonly, inlineonly,
				 fmt);
    }

    free_markercomps();
    fmt_free(fmt, 1);
}


/*
 * Entry point to show/display a single message
 */

static void
show_single_message (CT ct, char *form, int concatsw, int textonly,
		     int inlineonly, struct format *fmt)
{
    sigset_t set, oset;

    int status;

    /* Allow user executable bit so that temporary directories created by
     * the viewer (e.g., lynx) are going to be accessible */
    umask (ct->c_umask & ~(0100));

    /*
     * If you have a format file, then display
     * the message headers.
     */
    if (form)
	DisplayMsgHeader(ct, form, concatsw);

    /* Show the body of the message */
    show_switch (ct, 0, concatsw, textonly, inlineonly, fmt);

    if (ct->c_fp) {
	fclose (ct->c_fp);
	ct->c_fp = NULL;
    }
    if (ct->c_ceclosefnx)
	(*ct->c_ceclosefnx) (ct);

    /* block a few signals */
    sigemptyset (&set);
    sigaddset (&set, SIGHUP);
    sigaddset (&set, SIGINT);
    sigaddset (&set, SIGQUIT);
    sigaddset (&set, SIGTERM);
    sigprocmask (SIG_BLOCK, &set, &oset);

    while (!concatsw && wait (&status) != NOTOK) {
	pidcheck (status);
	continue;
    }

    /* reset the signal mask */
    sigprocmask (SIG_SETMASK, &oset, &set);

    flush_errors ();
}


/*
 * Use the mhlproc to show the header fields
 */

static void
DisplayMsgHeader (CT ct, char *form, int concatsw)
{
    pid_t child_id;
    int i, vecp;
    char **vec;
    char *file;

    vec = argsplit(mhlproc, &file, &vecp);
    vec[vecp++] = getcpy("-form");
    vec[vecp++] = getcpy(form);
    vec[vecp++] = getcpy("-nobody");
    vec[vecp++] = getcpy(ct->c_file);

    /*
     * If we've specified -(no)moreproc,
     * then just pass that along.
     */
    if (nomore || concatsw) {
	vec[vecp++] = getcpy("-nomoreproc");
    } else if (progsw) {
	vec[vecp++] = getcpy("-moreproc");
	vec[vecp++] = getcpy(progsw);
    }
    vec[vecp] = NULL;

    fflush (stdout);

    for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
	sleep (5);

    switch (child_id) {
    case NOTOK:
	adios ("fork", "unable to");
	/* NOTREACHED */

    case OK:
	execvp (file, vec);
	fprintf (stderr, "unable to exec ");
	perror (mhlproc);
	_exit (-1);
	/* NOTREACHED */

    default:
	pidcheck(pidwait(child_id, NOTOK));
	break;
    }

    arglist_free(file, vec);
}


/*
 * Switching routine.  Call the correct routine
 * based on content type.
 */

static int
show_switch (CT ct, int alternate, int concatsw, int textonly, int inlineonly,
	     struct format *fmt)
{
    switch (ct->c_type) {
	case CT_MULTIPART:
	    return show_multi (ct, alternate, concatsw, textonly,
			       inlineonly, fmt);

	case CT_MESSAGE:
	    switch (ct->c_subtype) {
		case MESSAGE_PARTIAL:
		    return show_partial (ct, alternate);

		case MESSAGE_EXTERNAL:
		    return show_external (ct, alternate, concatsw, textonly,
					  inlineonly, fmt);

		case MESSAGE_RFC822:
		default:
		    return show_message_rfc822 (ct, alternate, fmt);
	    }

	case CT_TEXT:
	    return show_text (ct, alternate, concatsw, fmt);

	case CT_AUDIO:
	case CT_IMAGE:
	case CT_VIDEO:
	case CT_APPLICATION:
	    return show_content (ct, alternate, textonly, inlineonly, fmt);

	default:
	    adios (NULL, "unknown content type %d", ct->c_type);
    }

    return 0;	/* NOT REACHED */
}


/*
 * Generic method for displaying content
 */

static int
show_content (CT ct, int alternate, int textonly, int inlineonly,
	      struct format *fmt)
{
    char *cp, buffer[BUFSIZ];
    CI ci = &ct->c_ctinfo;

    /*
     * If we're here, we are not a text type.  So we don't need to check
     * the content-type.
     */

    if (textonly || (inlineonly && !is_inline(ct))) {
	output_marker(ct, fmt, 1);
	return OK;
    }

    /* Check for invo_name-show-type/subtype */
    snprintf (buffer, sizeof(buffer), "%s-show-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, alternate, cp, NULL, fmt);

    /* Check for invo_name-show-type */
    snprintf (buffer, sizeof(buffer), "%s-show-%s", invo_name, ci->ci_type);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, alternate, cp, NULL, fmt);

    if ((cp = ct->c_showproc))
	return show_content_aux (ct, alternate, cp, NULL, fmt);

    /* complain if we are not a part of a multipart/alternative */
    if (!alternate)
	content_error (NULL, ct, "don't know how to display content");

    return NOTOK;
}


/*
 * Parse the display string for displaying generic content
 */

int
show_content_aux (CT ct, int alternate, char *cp, char *cracked, struct format *fmt)
{
    int fd;
    int xstdin = 0, xlist = 0;
    char *file, buffer[BUFSIZ];

    if (!ct->c_ceopenfnx) {
	if (!alternate)
	    content_error (NULL, ct, "don't know how to decode content");

	return NOTOK;
    }

    file = NULL;
    if ((fd = (*ct->c_ceopenfnx) (ct, &file)) == NOTOK)
	return NOTOK;
    if (ct->c_showproc && !strcmp (ct->c_showproc, "true"))
	return (alternate ? DONE : OK);

    if (! strcmp(invo_name, "mhshow")  &&
        ct->c_type == CT_TEXT  &&  ct->c_subtype == TEXT_PLAIN) {
        /* This has to be done after calling c_ceopenfnx, so
           unfortunately the type checks are necessary without
           some code rearrangement.  And to make this really ugly,
           only do it in mhshow, not mhfixmsg, mhn, or mhstore. */
        if (convert_content_charset (ct, &file) == OK) {
            (*ct->c_ceclosefnx) (ct);
            if ((fd = (*ct->c_ceopenfnx) (ct, &file)) == NOTOK)
                return NOTOK;
        } else {
            admonish (NULL, "unable to convert character set%s%s to %s",
                      ct->c_partno  ?  " of part "  :  "",
                      ct->c_partno  ?  ct->c_partno  :  "",
                      content_charset (ct));
        }
    }

    if (cracked) {
	strncpy (buffer, cp, sizeof(buffer));
	goto got_command;
    }

    if (parse_display_string (ct, cp, &xstdin, &xlist, file, buffer,
    			      sizeof(buffer) - 1, 0)) {
	admonish (NULL, "Buffer overflow constructing show command!\n");
	return NOTOK;
    }

got_command:
    return show_content_aux2 (ct, alternate, cracked, buffer,
			      fd, xlist, xstdin, fmt);
}


/*
 * Routine to actually display the content
 */

static int
show_content_aux2 (CT ct, int alternate, char *cracked, char *buffer,
                   int fd, int xlist, int xstdin, struct format *fmt)
{
    pid_t child_id;
    int i, vecp;
    char **vec, *file;

    if (debugsw || cracked) {
	fflush (stdout);

	fprintf (stderr, "%s msg %s", cracked ? "storing" : "show",
		 ct->c_file);
	if (ct->c_partno)
	    fprintf (stderr, " part %s", ct->c_partno);
	if (cracked)
	    fprintf (stderr, " using command (cd %s; %s)\n", cracked, buffer);
	else
	    fprintf (stderr, " using command %s\n", buffer);
    }

    if (xlist) {
	output_marker(ct, fmt, 0);
    }

    /*
     * If the command is a zero-length string, just write the output on
     * stdout.
     */

    if (buffer[0] == '\0') {
	char readbuf[BUFSIZ];
	ssize_t cc;

	if (fd == NOTOK) {
	    advise(NULL, "Cannot use NULL command to display content-type "
		   "%s/%s", ct->c_ctinfo.ci_type, ct->c_ctinfo.ci_subtype);
	    return NOTOK;
	}

	while ((cc = read(fd, readbuf, sizeof(readbuf))) > 0) {
	    fwrite(readbuf, sizeof(char), cc, stdout);
	}

	if (cc < 0) {
	    advise("read", "while reading text content");
	    return NOTOK;
	}

	return OK;
    }

    vec = argsplit(buffer, &file, &vecp);
    vec[vecp++] = NULL;

    fflush (stdout);

    for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
	sleep (5);
    switch (child_id) {
	case NOTOK:
	    advise ("fork", "unable to");
	    (*ct->c_ceclosefnx) (ct);
	    return NOTOK;

	case OK:
	    if (cracked)
		chdir (cracked);
	    if (!xstdin)
		dup2 (fd, 0);
	    close (fd);
	    execvp (file, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (buffer);
	    _exit (-1);
	    /* NOTREACHED */

	default:
	    arglist_free(file, vec);

	    pidcheck (pidXwait (child_id, NULL));

	    if (fd != NOTOK)
		(*ct->c_ceclosefnx) (ct);
	    return (alternate ? DONE : OK);
    }
}


/*
 * show content of type "text"
 */

static int
show_text (CT ct, int alternate, int concatsw, struct format *fmt)
{
    char *cp, buffer[BUFSIZ];
    CI ci = &ct->c_ctinfo;

    /* Check for invo_name-show-type/subtype */
    snprintf (buffer, sizeof(buffer), "%s-show-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, alternate, cp, NULL, fmt);

    /* Check for invo_name-show-type */
    snprintf (buffer, sizeof(buffer), "%s-show-%s", invo_name, ci->ci_type);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, alternate, cp, NULL, fmt);

    /*
     * Use default method if content is text/plain, or if
     * if it is not a text part of a multipart/alternative
     */
    if (!alternate || ct->c_subtype == TEXT_PLAIN) {
	if (concatsw) {
	    if (ct->c_termproc)
		snprintf(buffer, sizeof(buffer), "%%lcat");
	    else
		snprintf(buffer, sizeof(buffer), "%%l");
	} else
	    snprintf (buffer, sizeof(buffer), "%%l%s %%F", progsw ? progsw :
		      moreproc && *moreproc ? moreproc : DEFAULT_PAGER);
	cp = (ct->c_showproc = add (buffer, NULL));
	return show_content_aux (ct, alternate, cp, NULL, fmt);
    }

    return NOTOK;
}


/*
 * show message body of type "multipart"
 */

static int
show_multi (CT ct, int alternate, int concatsw, int textonly, int inlineonly,
	    struct format *fmt)
{
    char *cp, buffer[BUFSIZ];
    CI ci = &ct->c_ctinfo;

    /* Check for invo_name-show-type/subtype */
    snprintf (buffer, sizeof(buffer), "%s-show-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_multi_aux (ct, alternate, cp, fmt);

    /* Check for invo_name-show-type */
    snprintf (buffer, sizeof(buffer), "%s-show-%s", invo_name, ci->ci_type);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_multi_aux (ct, alternate, cp, fmt);

    if ((cp = ct->c_showproc))
	return show_multi_aux (ct, alternate, cp, fmt);

    /*
     * Use default method to display this multipart content.  Even
     * unknown types are displayable, since they're treated as mixed
     * per RFC 2046.
     */
    return show_multi_internal (ct, alternate, concatsw, textonly,
				inlineonly, fmt);
}


/*
 * show message body of subtypes of multipart that
 * we understand directly (mixed, alternate, etc...)
 */

static int
show_multi_internal (CT ct, int alternate, int concatsw, int textonly,
		     int inlineonly, struct format *fmt)
{
    int	alternating, nowalternate, result;
    struct multipart *m = (struct multipart *) ct->c_ctparams;
    struct part *part;
    int any_part_ok;
    CT p;

    alternating = 0;
    nowalternate = alternate;

    if (ct->c_subtype == MULTI_ALTERNATE) {
	nowalternate = 1;
	alternating  = 1;
    }

/*
 * alternate   -> we are a part inside an multipart/alternative
 * alternating -> we are a multipart/alternative
 */

    result = alternate ? NOTOK : OK;
    any_part_ok = 0;

    for (part = m->mp_parts; part; part = part->mp_next) {
	p = part->mp_part;

	if (part_ok (p, 1) && type_ok (p, 1)) {
	    int	inneresult;

	    any_part_ok = 1;

	    inneresult = show_switch (p, nowalternate, concatsw, textonly,
				      inlineonly, fmt);
	    switch (inneresult) {
		case NOTOK:
		    if (alternate && !alternating) {
			result = NOTOK;
			goto out;
		    }
		    continue;

		case OK:
		case DONE:
		    if (alternating) {
			result = DONE;
			break;
		    }
		    if (alternate) {
			alternate = nowalternate = 0;
			if (result == NOTOK)
			    result = inneresult;
		    }
		    continue;
	    }
	    break;
	}
    }

    if (alternating && !part && any_part_ok) {
	if (!alternate)
	    content_error (NULL, ct, "don't know how to display any of the contents");
	result = NOTOK;
	goto out;
    }

out:
    return result;
}


/*
 * Parse display string for multipart content
 * and use external program to display it.
 */

static int
show_multi_aux (CT ct, int alternate, char *cp, struct format *fmt)
{
    /* xstdin is only used in the call to parse_display_string():
       its value is ignored in the function. */
    int xstdin = 0, xlist = 0;
    char *file, buffer[BUFSIZ];
    struct multipart *m = (struct multipart *) ct->c_ctparams;
    struct part *part;
    CT p;

    for (part = m->mp_parts; part; part = part->mp_next) {
	p = part->mp_part;

	if (!p->c_ceopenfnx) {
	    if (!alternate)
		content_error (NULL, p, "don't know how to decode content");
	    return NOTOK;
	}

	if (p->c_storage == NULL) {
	    file = NULL;
	    if ((*p->c_ceopenfnx) (p, &file) == NOTOK)
		return NOTOK;

	    p->c_storage = add (file, NULL);

	    if (p->c_showproc && !strcmp (p->c_showproc, "true"))
		return (alternate ? DONE : OK);
	    (*p->c_ceclosefnx) (p);
	}
    }

    if (parse_display_string (ct, cp, &xstdin, &xlist, file,
			      buffer, sizeof(buffer) - 1, 1)) {
	admonish (NULL, "Buffer overflow constructing show command!\n");
	return NOTOK;
    }

    return show_content_aux2 (ct, alternate, NULL, buffer, NOTOK, xlist, 0, fmt);
}


/*
 * show content of type "message/rfc822"
 */

static int
show_message_rfc822 (CT ct, int alternate, struct format *fmt)
{
    char *cp, buffer[BUFSIZ];
    CI ci = &ct->c_ctinfo;

    /* Check for invo_name-show-type/subtype */
    snprintf (buffer, sizeof(buffer), "%s-show-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, alternate, cp, NULL, fmt);

    /* Check for invo_name-show-type */
    snprintf (buffer, sizeof(buffer), "%s-show-%s", invo_name, ci->ci_type);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, alternate, cp, NULL, fmt);

    if ((cp = ct->c_showproc))
	return show_content_aux (ct, alternate, cp, NULL, fmt);

    /* default method for message/rfc822 */
    if (ct->c_subtype == MESSAGE_RFC822) {
	cp = (ct->c_showproc = add ("%pshow -file %F", NULL));
	return show_content_aux (ct, alternate, cp, NULL, fmt);
    }

    /* complain if we are not a part of a multipart/alternative */
    if (!alternate)
	content_error (NULL, ct, "don't know how to display content");

    return NOTOK;
}


/*
 * Show content of type "message/partial".
 */

static int
show_partial (CT ct, int alternate)
{
    NMH_UNUSED (alternate);

    content_error (NULL, ct,
	"in order to display this message, you must reassemble it");
    return NOTOK;
}


/*
 * Show content of type "message/external".
 *
 * THE ERROR CHECKING IN THIS ONE IS NOT DONE YET.
 */

static int
show_external (CT ct, int alternate, int concatsw, int textonly, int inlineonly,
	       struct format *fmt)
{
    struct exbody *e = (struct exbody *) ct->c_ctparams;
    CT p = e->eb_content;

    if (!type_ok (p, 0))
	return OK;

    return show_switch (p, alternate, concatsw, textonly, inlineonly, fmt);
}


static int
parse_display_string (CT ct, char *cp, int *xstdin, int *xlist,
                      char *file, char *buffer, size_t buflen,
                      int multipart) {
    int len, quoted = 0;
    char *bp = buffer, *pp;
    CI ci = &ct->c_ctinfo;

    bp[0] = bp[buflen] = '\0';

    for ( ; *cp && buflen > 0; cp++) {
	if (*cp == '%') {
	    pp = bp;

	    switch (*++cp) {
	    case 'a':
		/* insert parameters from Content-Type field */
	    {
		PM pm;
		char *s = "";

		for (pm = ci->ci_first_pm; pm; pm = pm->pm_next) {
		    snprintf (bp, buflen, "%s%s=\"%s\"", s, pm->pm_name,
			      get_param_value(pm, '?'));
		    len = strlen (bp);
		    bp += len;
		    buflen -= len;
		    s = " ";
		}
	    }
	    break;

	    case 'd':
		/* insert content description */
		if (ct->c_descr) {
		    char *s;

		    s = trimcpy (ct->c_descr);
		    strncpy (bp, s, buflen);
		    free (s);
		}
		break;

	    case 'e':
		/* no longer implemented */
		break;

	    case 'F':
		/* %f, and stdin is terminal not content */
		*xstdin = 1;
		/* and fall... */

	    case 'f':
		if (multipart) {
		    /* insert filename(s) containing content */
		    struct multipart *m = (struct multipart *) ct->c_ctparams;
		    struct part *part;
		    char *s = "";
		    CT p;

		    for (part = m->mp_parts; part; part = part->mp_next) {
			p = part->mp_part;

			snprintf (bp, buflen, "%s%s", s, p->c_storage);
			len = strlen (bp);
			bp += len;
			buflen -= len;
			s = " ";
		    }
		} else {
		    /* insert filename containing content */
		    snprintf (bp, buflen, "%s", file);

		    /*
		     * Old comments below are left here for posterity.
		     * This was/is tricky.
		     */
		    /* since we've quoted the file argument, set things up
		     * to look past it, to avoid problems with the quoting
		     * logic below.  (I know, I should figure out what's
		     * broken with the quoting logic, but..)
		     */
		    /*
		     * Here's the email that submitted the patch with
		     * the comment above:
		     *   https://www.mail-archive.com/nmh-workers@mhost.com/
		     *     msg00288.html
		     * I can't tell from that exactly what was broken,
		     * beyond misquoting of the filename.  The profile
		     * had appearances of %F both with and without quotes.
		     * The unquoted ones should have been quoted by the
		     * code below.
		     * The fix was to always quote the filename.  But
		     * that broke '%F' because it expanded to ''filename''.
		     */
		    /*
		     * Old comments above are left here for posterity.
		     * The quoting below should work properly now.
		     */
		}
		break;

	    case 'p':
		/* No longer supported */
		/* and fall... */

	    case 'l':
		/* display listing prior to displaying content */
		*xlist = !nolist;
		break;

	    case 's':
		/* insert subtype of content */
		strncpy (bp, ci->ci_subtype, buflen);
		break;

	    case '%':
		/* insert character % */
		goto raw;

	    case '{' : {
		const char *closing_brace = strchr(cp, '}');

		if (closing_brace) {
		    const size_t param_len = closing_brace - cp - 1;
		    char *param = mh_xmalloc(param_len + 1);
		    char *value;

		    (void) strncpy(param, cp + 1, param_len);
		    param[param_len] = '\0';
		    value = get_param(ci->ci_first_pm, param, '?', 0);
		    free(param);

		    cp += param_len + 1; /* Skip both braces, too. */

		    if (value) {
			/* %{param} is set in the Content-Type header.
			   After the break below, quote it if necessary. */
			(void) strncpy(bp, value, buflen);
			free(value);
		    } else {
			/* %{param} not found, so skip it completely.  cp
			   was advanced above. */
			continue;
		    }
		} else {
		    /* This will get confused if there are multiple %{}'s,
		       but its real purpose is to avoid doing bad things
		       above if a closing brace wasn't found. */
		    admonish(NULL,
			     "no closing brace for display string escape %s",
			     cp);
		}
		break;
	    }

	    default:
		*bp++ = *--cp;
		*bp = '\0';
		buflen--;
		continue;
	    }
	    len = strlen (bp);
	    bp += len;
	    buflen -= len;
	    *bp = '\0';

	    /* Did we actually insert something? */
	    if (bp != pp) {
		/* Insert single quote if not inside quotes already */
		if (!quoted && buflen) {
		    len = strlen (pp);
		    memmove (pp + 1, pp, len+1);
		    *pp++ = '\'';
		    buflen--;
		    bp++;
		    quoted = 1;
		}
		/* Escape existing quotes */
		while ((pp = strchr (pp, '\'')) && buflen > 3) {
		    len = strlen (pp++);
		    if (quoted) {
			/* Quoted.  Let this quote close that quoting.
			   Insert an escaped quote to replace it and
			   another quote to reopen quoting, which will be
			   closed below. */
			memmove (pp + 2, pp, len);
			*pp++ = '\\';
			*pp++ = '\'';
			buflen -= 2;
			bp += 2;
			quoted = 0;
		    } else {
			/* Not quoted.  This should not be reached with
			   the current code, but handle the condition
			   in case the code changes.  Just escape the
			   quote. */
			memmove (pp, pp-1, len+1);
			*(pp++-1) = '\\';
			buflen -= 1;
			bp += 1;
		    }
		}
		/* If pp is still set, that means we ran out of space. */
		if (pp)
		    buflen = 0;
		/* Close quoting. */
		if (quoted && buflen) {
		    /* See if we need to close the quote by looking
		       for an odd number of unescaped close quotes in
		       the remainder of the display string. */
		    int found_quote = 0, escaped = 0;
		    char *c;

		    for (c = cp+1; *c; ++c) {
			if (*c == '\\') {
			    escaped = ! escaped;
			} else {
			    if (escaped) {
				escaped = 0;
			    } else {
				if (*c == '\'') {
				    found_quote = ! found_quote;
				}
			    }
			}
		    }
		    if (! found_quote) {
			*bp++ = '\'';
			buflen--;
			quoted = 0;
		    }
		}
	    }
	} else {
raw:
	    *bp++ = *cp;
	    buflen--;

	    if (*cp == '\'')
		quoted = !quoted;
	}

	*bp = '\0';
    }

    if (buflen <= 0 ||
        (ct->c_termproc && buflen <= strlen(ct->c_termproc))) {
	/* content_error would provide a more useful error message
	 * here, except that if we got overrun, it probably would
	 * too.
	 */
	return NOTOK;
    }

    /* use charset string to modify display method */
    if (ct->c_termproc) {
	char term[BUFSIZ];

	strncpy (term, buffer, sizeof(term));
	snprintf (buffer, buflen, ct->c_termproc, term);
    }

    return OK;
}


int
convert_charset (CT ct, char *dest_charset, int *message_mods) {
    char *src_charset = content_charset (ct);
    int status = OK;

    /* norm_charmap() is case sensitive. */
    char *src_charset_u = upcase (src_charset);
    char *dest_charset_u = upcase (dest_charset);
    int different_charsets =
        strcmp (norm_charmap (src_charset), norm_charmap (dest_charset));

    free (dest_charset_u);
    free (src_charset_u);

    if (different_charsets) {
#ifdef HAVE_ICONV
        iconv_t conv_desc = NULL;
        char *dest;
        int fd = -1;
        char **file = NULL;
        FILE **fp = NULL;
        size_t begin;
        size_t end;
        int opened_input_file = 0;
        char src_buffer[BUFSIZ];
	size_t dest_buffer_size = BUFSIZ;
	char *dest_buffer = mh_xmalloc(dest_buffer_size);
        HF hf;
        char *tempfile;

        if ((conv_desc = iconv_open (dest_charset, src_charset)) ==
            (iconv_t) -1) {
            advise (NULL, "Can't convert %s to %s", src_charset, dest_charset);
            return NOTOK;
        }

        if ((tempfile = m_mktemp2 (NULL, invo_name, &fd, NULL)) == NULL) {
            adios (NULL, "unable to create temporary file in %s",
                   get_temp_dir());
        }
        dest = add (tempfile, NULL);

        if (ct->c_cefile.ce_file) {
            file = &ct->c_cefile.ce_file;
            fp = &ct->c_cefile.ce_fp;
            begin = end = 0;
        } else if (ct->c_file) {
            file = &ct->c_file;
            fp = &ct->c_fp;
            begin = (size_t) ct->c_begin;
            end = (size_t) ct->c_end;
        } /* else no input file: shouldn't happen */

        if (file  &&  *file  &&  fp) {
            if (! *fp) {
                if ((*fp = fopen (*file, "r")) == NULL) {
                    advise (*file, "unable to open for reading");
                    status = NOTOK;
                } else {
                    opened_input_file = 1;
                }
            }
        }

        if (fp  &&  *fp) {
            size_t inbytes;
            size_t bytes_to_read =
                end > 0 && end > begin  ?  end - begin  :  sizeof src_buffer;

            fseeko (*fp, begin, SEEK_SET);
            while ((inbytes = fread (src_buffer, 1,
                                     min (bytes_to_read, sizeof src_buffer),
                                     *fp)) > 0) {
                ICONV_CONST char *ib = src_buffer;
                char *ob = dest_buffer;
                size_t outbytes = dest_buffer_size;
                size_t outbytes_before = outbytes;

                if (end > 0) bytes_to_read -= inbytes;

iconv_start:
                if (iconv (conv_desc, &ib, &inbytes, &ob, &outbytes) ==
                    (size_t) -1) {
		    if (errno == E2BIG) {
			/*
			 * Bump up the buffer by at least a factor of 2
			 * over what we need.
			 */
			size_t bumpup = inbytes * 2, ob_off = ob - dest_buffer;
			dest_buffer_size += bumpup;
			dest_buffer = mh_xrealloc(dest_buffer,
						  dest_buffer_size);
			ob = dest_buffer + ob_off;
			outbytes += bumpup;
			outbytes_before += bumpup;
			goto iconv_start;
		    }
                    status = NOTOK;
                    break;
                } else {
                    write (fd, dest_buffer, outbytes_before - outbytes);
                }
            }

            if (opened_input_file) {
                fclose (*fp);
                *fp = NULL;
            }
        }

        iconv_close (conv_desc);
        close (fd);

        if (status == OK) {
            /* Replace the decoded file with the converted one. */
            if (ct->c_cefile.ce_file) {
                if (ct->c_cefile.ce_unlink) {
                    (void) m_unlink (ct->c_cefile.ce_file);
                }
                free (ct->c_cefile.ce_file);
            }
            ct->c_cefile.ce_file = dest;
            ct->c_cefile.ce_unlink = 1;

            ++*message_mods;

            /* Update ct->c_ctline. */
            if (ct->c_ctline) {
                char *ctline = concat(" ", ct->c_ctinfo.ci_type, "/",
				      ct->c_ctinfo.ci_subtype, NULL);
		char *outline;

		replace_param(&ct->c_ctinfo.ci_first_pm,
			      &ct->c_ctinfo.ci_last_pm, "charset",
			      dest_charset, 0);
		outline = output_params(strlen(TYPE_FIELD) + 1 + strlen(ctline),
					ct->c_ctinfo.ci_first_pm, NULL, 0);
		if (outline) {
		    ctline = add(outline, ctline);
		    free(outline);
		}

                free (ct->c_ctline);
                ct->c_ctline = ctline;
            } /* else no CT line, which is odd */

            /* Update Content-Type header field. */
            for (hf = ct->c_first_hf; hf; hf = hf->next) {
                if (! strcasecmp (TYPE_FIELD, hf->name)) {
                    char *ctline = concat (ct->c_ctline, "\n", NULL);

                    free (hf->value);
                    hf->value = ctline;
                    break;
                }
            }
        } else {
            (void) m_unlink (dest);
        }
	free(dest_buffer);
#else  /* ! HAVE_ICONV */
        NMH_UNUSED (message_mods);

        advise (NULL, "Can't convert %s to %s without iconv", src_charset,
                dest_charset);
        errno = ENOSYS;
        status = NOTOK;
#endif /* ! HAVE_ICONV */
    }

    return status;
}


static int
convert_content_charset (CT ct, char **file) {
#ifdef HAVE_ICONV
    /* Using current locale, see if the content needs to be converted. */

    /* content_charset() cannot return NULL. */
    char *charset = content_charset (ct);

    if (! check_charset (charset, strlen (charset))) {
        int unused = 0;

        if (convert_charset (ct, get_charset (), &unused) == 0) {
            *file = ct->c_cefile.ce_file;
        } else {
            return NOTOK;
        }
    }
#else  /* ! HAVE_ICONV */
    NMH_UNUSED (ct);
    NMH_UNUSED (file);
#endif /* ! HAVE_ICONV */

    return OK;
}

/*
 * Compile our format string and save any parameters we care about.
 */

#define DEFAULT_MARKER "[ part %{part} - %{content-type} - %<{description}" \
		       "%{description}%?{cdispo-filename}%{cdispo-filename}" \
		       "%|%{ctype-name}%> %<(unseen)\\(suppressed\\)%> ]"

static struct format *
compile_marker(char *markerform)
{
    struct format *fmt;
    char *fmtstring;
    struct comp *comp = NULL;
    unsigned int bucket;
    struct param_comp_list *pc_entry;

    fmtstring = new_fs(markerform, NULL, DEFAULT_MARKER);

    (void) fmt_compile(fmtstring, &fmt, 1);
    free(fmtstring);

    /*
     * Things we care about:
     *
     * part		- Part name (e.g., 1.1)
     * content-type	- Content-Type
     * description	- Content-Description
     * disposition	- Content-Disposition (inline, attachment)
     * ctype-<param>	- Content-Type parameter
     * cdispo-<param>	- Content-Disposition parameter
     */

    while ((comp = fmt_nextcomp(comp, &bucket)) != NULL) {
	if (strcasecmp(comp->c_name, "part") == 0) {
	    part_comp = comp;
	} else if (strcasecmp(comp->c_name, "content-type") == 0) {
	    ctype_comp = comp;
	} else if (strcasecmp(comp->c_name, "description") == 0) {
	    description_comp = comp;
	} else if (strcasecmp(comp->c_name, "disposition") == 0) {
	    dispo_comp = comp;
	} else if (strncasecmp(comp->c_name, "ctype-", 6) == 0 &&
		   strlen(comp->c_name) > 6) {
	    pc_entry = mh_xmalloc(sizeof(*pc_entry));
	    pc_entry->param = getcpy(comp->c_name + 6);
	    pc_entry->comp = comp;
	    pc_entry->next = ctype_pc_list;
	    ctype_pc_list = pc_entry;
	} else if (strncasecmp(comp->c_name, "cdispo-", 7) == 0 &&
		   strlen(comp->c_name) > 7) {
	    pc_entry = mh_xmalloc(sizeof(*pc_entry));
	    pc_entry->param = getcpy(comp->c_name + 7);
	    pc_entry->comp = comp;
	    pc_entry->next = dispo_pc_list;
	    dispo_pc_list = pc_entry;
	}
    }

    return fmt;
}

/*
 * Output on stdout an appropriate marker for this content, using mh-format
 */

static void
output_marker(CT ct, struct format *fmt, int hidden)
{
    char outbuf[BUFSIZ];
    struct param_comp_list *pcentry;
    int dat[5];

    /*
     * Grab any items we care about.
     */

    if (ctype_comp && ct->c_ctinfo.ci_type) {
	ctype_comp->c_text = concat(ct->c_ctinfo.ci_type, "/",
				    ct->c_ctinfo.ci_subtype, NULL);
    }

    if (part_comp && ct->c_partno) {
	part_comp->c_text = getcpy(ct->c_partno);
    }

    if (description_comp && ct->c_descr) {
	description_comp->c_text = getcpy(ct->c_descr);
    }

    if (dispo_comp && ct->c_dispo_type) {
	dispo_comp->c_text = getcpy(ct->c_dispo_type);
    }

    for (pcentry = ctype_pc_list; pcentry != NULL; pcentry = pcentry->next) {
	pcentry->comp->c_text = get_param(ct->c_ctinfo.ci_first_pm,
					  pcentry->param, '?', 0);
    }

    for (pcentry = dispo_pc_list; pcentry != NULL; pcentry = pcentry->next) {
	pcentry->comp->c_text = get_param(ct->c_dispo_first,
					  pcentry->param, '?', 0);
    }

    /* make the part's hidden aspect available by overloading the
     * %(unseen) function.  see comments in h/fmt_scan.h.
     */
    dat[0] = dat[1] = dat[2] = dat[3] = 0;
    dat[4] = hidden;

    fmt_scan(fmt, outbuf, sizeof(outbuf), sizeof(outbuf), dat, NULL);

    fputs(outbuf, stdout);

    fmt_freecomptext();
}

/*
 * Reset (and free) any of the saved marker text
 */

static void
free_markercomps(void)
{
    struct param_comp_list *pc_entry, *pc2;

    part_comp = NULL;
    ctype_comp = NULL;
    description_comp = NULL;
    dispo_comp = NULL;

    for (pc_entry = ctype_pc_list; pc_entry != NULL; ) {
	free(pc_entry->param);
	pc2 = pc_entry->next;
	free(pc_entry);
	pc_entry = pc2;
    }

    for (pc_entry = dispo_pc_list; pc_entry != NULL; ) {
	free(pc_entry->param);
	pc2 = pc_entry->next;
	free(pc_entry);
	pc_entry = pc2;
    }
}

/*
 * Exit if the display process returned with a nonzero exit code, or terminated
 * with a SIGQUIT signal.
 */

static int
pidcheck (int status)
{
    if ((status & 0xff00) == 0xff00 || (status & 0x007f) != SIGQUIT)
	return status;

    fflush (stdout);
    fflush (stderr);
    done (1);
    return 1;
}
