
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
#include <h/utils.h>
#ifdef HAVE_ICONV
#   include <iconv.h>
#endif /* ! HAVE_ICONV */

extern int debugsw;

int pausesw  = 1;
int serialsw = 0;
int nolist   = 0;

char *progsw = NULL;

/* flags for moreproc/header display */
int nomore   = 0;
char *formsw = NULL;

pid_t xpid = 0;

static sigjmp_buf intrenv;


/* mhmisc.c */
int part_ok (CT, int);
int type_ok (CT, int);
void content_error (char *, CT, char *, ...);
void flush_errors (void);

/* mhlistsbr.c */
int list_switch (CT, int, int, int, int);
int list_content (CT, int, int, int, int);

/*
 * prototypes
 */
void show_all_messages (CT *);
int show_content_aux (CT, int, int, char *, char *);

/*
 * static prototypes
 */
static void show_single_message (CT, char *);
static void DisplayMsgHeader (CT, char *);
static int show_switch (CT, int, int);
static int show_content (CT, int, int);
static int show_content_aux2 (CT, int, int, char *, char *, int, int, int, int,
                              int);
static int show_text (CT, int, int);
static int show_multi (CT, int, int);
static int show_multi_internal (CT, int, int);
static int show_multi_aux (CT, int, int, char *);
static int show_message_rfc822 (CT, int, int);
static int show_partial (CT, int, int);
static int show_external (CT, int, int);
static int parse_display_string (CT, char *, int *, int *, int *, int *, char *,
                                 char *, size_t, int multipart);
static int convert_content_charset (CT, char **);
static int parameter_value (CI, const char *, const char *, const char **);
static void intrser (int);


/*
 * Top level entry point to show/display a group of messages
 */

void
show_all_messages (CT *cts)
{
    CT ct, *ctp;

    /*
     * If form is not specified, then get default form
     * for showing headers of MIME messages.
     */
    if (!formsw)
	formsw = getcpy (etcpath ("mhl.headers"));

    /*
     * If form is "mhl.null", suppress display of header.
     */
    if (!strcmp (formsw, "mhl.null"))
	formsw = NULL;

    for (ctp = cts; *ctp; ctp++) {
	ct = *ctp;

	/* if top-level type is ok, then display message */
	if (type_ok (ct, 1))
	    show_single_message (ct, formsw);
    }
}


/*
 * Entry point to show/display a single message
 */

static void
show_single_message (CT ct, char *form)
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
	DisplayMsgHeader(ct, form);
    else
	xpid = 0;

    /* Show the body of the message */
    show_switch (ct, 1, 0);

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

    while (wait (&status) != NOTOK) {
	pidcheck (status);
	continue;
    }

    /* reset the signal mask */
    sigprocmask (SIG_SETMASK, &oset, &set);

    xpid = 0;
    flush_errors ();
}


/*
 * Use the mhlproc to show the header fields
 */

static void
DisplayMsgHeader (CT ct, char *form)
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
    if (nomore) {
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
	xpid = -child_id;
	break;
    }

    arglist_free(file, vec);
}


/*
 * Switching routine.  Call the correct routine
 * based on content type.
 */

static int
show_switch (CT ct, int serial, int alternate)
{
    switch (ct->c_type) {
	case CT_MULTIPART:
	    return show_multi (ct, serial, alternate);

	case CT_MESSAGE:
	    switch (ct->c_subtype) {
		case MESSAGE_PARTIAL:
		    return show_partial (ct, serial, alternate);

		case MESSAGE_EXTERNAL:
		    return show_external (ct, serial, alternate);

		case MESSAGE_RFC822:
		default:
		    return show_message_rfc822 (ct, serial, alternate);
	    }

	case CT_TEXT:
	    return show_text (ct, serial, alternate);

	case CT_AUDIO:
	case CT_IMAGE:
	case CT_VIDEO:
	case CT_APPLICATION:
	    return show_content (ct, serial, alternate);

	default:
	    adios (NULL, "unknown content type %d", ct->c_type);
    }

    return 0;	/* NOT REACHED */
}


/*
 * Generic method for displaying content
 */

static int
show_content (CT ct, int serial, int alternate)
{
    char *cp, buffer[BUFSIZ];
    CI ci = &ct->c_ctinfo;

    /* Check for invo_name-show-type/subtype */
    snprintf (buffer, sizeof(buffer), "%s-show-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, serial, alternate, cp, NULL);

    /* Check for invo_name-show-type */
    snprintf (buffer, sizeof(buffer), "%s-show-%s", invo_name, ci->ci_type);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, serial, alternate, cp, NULL);

    if ((cp = ct->c_showproc))
	return show_content_aux (ct, serial, alternate, cp, NULL);

    /* complain if we are not a part of a multipart/alternative */
    if (!alternate)
	content_error (NULL, ct, "don't know how to display content");

    return NOTOK;
}


/*
 * Parse the display string for displaying generic content
 */

int
show_content_aux (CT ct, int serial, int alternate, char *cp, char *cracked)
{
    int fd;
    int xstdin = 0, xlist = 0, xpause = 0, xtty = 0;
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
        if (convert_content_charset (ct, &file) != OK) {
            admonish (NULL, "unable to convert character set%s to %s",
                      ct->c_partno  ?  "of part "  :  "",
                      ct->c_partno  ?  ct->c_partno  :  "",
                      content_charset (ct));
        }
    }

    if (cracked) {
	strncpy (buffer, cp, sizeof(buffer));
	goto got_command;
    }

    if (parse_display_string (ct, cp, &xstdin, &xlist, &xpause, &xtty, file,
			      buffer, sizeof(buffer) - 1, 0)) {
	admonish (NULL, "Buffer overflow constructing show command!\n");
	return NOTOK;
    }

got_command:
    return show_content_aux2 (ct, serial, alternate, cracked, buffer,
			      fd, xlist, xpause, xstdin, xtty);
}


/*
 * Routine to actually display the content
 */

static int
show_content_aux2 (CT ct, int serial, int alternate, char *cracked, char *buffer,
                   int fd, int xlist, int xpause, int xstdin, int xtty)
{
    pid_t child_id;
    int i;
    char *vec[4];

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

    if (xpid < 0 || (xtty && xpid)) {
	if (xpid < 0)
	    xpid = -xpid;
	pidcheck(pidwait (xpid, NOTOK));
	xpid = 0;
    }

    if (xlist) {
	char prompt[BUFSIZ];

	if (ct->c_type == CT_MULTIPART)
	    list_content (ct, -1, 1, 0, 0);
	else
	    list_switch (ct, -1, 1, 0, 0);

	if (xpause && isatty (fileno (stdout))) {
	    int	intr;
	    SIGNAL_HANDLER istat;

	    if (SOprintf ("Press <return> to show content..."))
		printf ("Press <return> to show content...");

	    istat = SIGNAL (SIGINT, intrser);
	    if ((intr = sigsetjmp (intrenv, 1)) == OK) {
		fflush (stdout);
		prompt[0] = 0;
		read (fileno (stdout), prompt, sizeof(prompt));
	    }
	    SIGNAL (SIGINT, istat);
	    if (intr != OK || prompt[0] == 'n') {
		(*ct->c_ceclosefnx) (ct);
		return (alternate ? DONE : NOTOK);
	    }
	    if (prompt[0] == 'q') done(OK);
	}
    }

    vec[0] = "/bin/sh";
    vec[1] = "-c";
    vec[2] = buffer;
    vec[3] = NULL;

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
	    execvp ("/bin/sh", vec);
	    fprintf (stderr, "unable to exec ");
	    perror ("/bin/sh");
	    _exit (-1);
	    /* NOTREACHED */

	default:
	    if (!serial) {
		ct->c_pid = child_id;
		if (xtty)
		    xpid = child_id;
	    } else {
		pidcheck (pidXwait (child_id, NULL));
	    }

	    if (fd != NOTOK)
		(*ct->c_ceclosefnx) (ct);
	    return (alternate ? DONE : OK);
    }
}


/*
 * show content of type "text"
 */

static int
show_text (CT ct, int serial, int alternate)
{
    char *cp, buffer[BUFSIZ];
    CI ci = &ct->c_ctinfo;

    /* Check for invo_name-show-type/subtype */
    snprintf (buffer, sizeof(buffer), "%s-show-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, serial, alternate, cp, NULL);

    /* Check for invo_name-show-type */
    snprintf (buffer, sizeof(buffer), "%s-show-%s", invo_name, ci->ci_type);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, serial, alternate, cp, NULL);

    /*
     * Use default method if content is text/plain, or if
     * if it is not a text part of a multipart/alternative
     */
    if (!alternate || ct->c_subtype == TEXT_PLAIN) {
	snprintf (buffer, sizeof(buffer), "%%p%s '%%F'", progsw ? progsw :
		moreproc && *moreproc ? moreproc : DEFAULT_PAGER);
	cp = (ct->c_showproc = add (buffer, NULL));
	return show_content_aux (ct, serial, alternate, cp, NULL);
    }

    return NOTOK;
}


/*
 * show message body of type "multipart"
 */

static int
show_multi (CT ct, int serial, int alternate)
{
    char *cp, buffer[BUFSIZ];
    CI ci = &ct->c_ctinfo;

    /* Check for invo_name-show-type/subtype */
    snprintf (buffer, sizeof(buffer), "%s-show-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_multi_aux (ct, serial, alternate, cp);

    /* Check for invo_name-show-type */
    snprintf (buffer, sizeof(buffer), "%s-show-%s", invo_name, ci->ci_type);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_multi_aux (ct, serial, alternate, cp);

    if ((cp = ct->c_showproc))
	return show_multi_aux (ct, serial, alternate, cp);

    /*
     * Use default method to display this multipart content.  Even
     * unknown types are displayable, since they're treated as mixed
     * per RFC 2046.
     */
    return show_multi_internal (ct, serial, alternate);
}


/*
 * show message body of subtypes of multipart that
 * we understand directly (mixed, alternate, etc...)
 */

static int
show_multi_internal (CT ct, int serial, int alternate)
{
    int	alternating, nowalternate, nowserial, result;
    struct multipart *m = (struct multipart *) ct->c_ctparams;
    struct part *part;
    CT p;
    sigset_t set, oset;

    alternating = 0;
    nowalternate = alternate;

    if (ct->c_subtype == MULTI_PARALLEL) {
	nowserial = serialsw;
    } else if (ct->c_subtype == MULTI_ALTERNATE) {
	nowalternate = 1;
	alternating  = 1;
	nowserial = serial;
    } else {
	/*
	 * multipart/mixed
	 * mutlipart/digest
	 * unknown subtypes of multipart (treat as mixed per rfc2046)
	 */
	nowserial = serial;
    }

    /* block a few signals */
    if (!nowserial) {
	sigemptyset (&set);
	sigaddset (&set, SIGHUP);
	sigaddset (&set, SIGINT);
	sigaddset (&set, SIGQUIT);
	sigaddset (&set, SIGTERM);
	sigprocmask (SIG_BLOCK, &set, &oset);
    }

/*
 * alternate   -> we are a part inside an multipart/alternative
 * alternating -> we are a multipart/alternative
 */

    result = alternate ? NOTOK : OK;

    for (part = m->mp_parts; part; part = part->mp_next) {
	p = part->mp_part;

	if (part_ok (p, 1) && type_ok (p, 1)) {
	    int	inneresult;

	    inneresult = show_switch (p, nowserial, nowalternate);
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

    if (alternating && !part) {
	if (!alternate)
	    content_error (NULL, ct, "don't know how to display any of the contents");
	result = NOTOK;
	goto out;
    }

    if (serial && !nowserial) {
	pid_t pid;
	int kids;
	int status;

	kids = 0;
	for (part = m->mp_parts; part; part = part->mp_next) {
	    p = part->mp_part;

	    if (p->c_pid > OK) {
		if (kill (p->c_pid, 0) == NOTOK)
		    p->c_pid = 0;
		else
		    kids++;
	    }
	}

	while (kids > 0 && (pid = wait (&status)) != NOTOK) {
	    pidcheck (status);

	    for (part = m->mp_parts; part; part = part->mp_next) {
		p = part->mp_part;

		if (xpid == pid)
		    xpid = 0;
		if (p->c_pid == pid) {
		    p->c_pid = 0;
		    kids--;
		    break;
		}
	    }
	}
    }

out:
    if (!nowserial) {
	/* reset the signal mask */
	sigprocmask (SIG_SETMASK, &oset, &set);
    }

    return result;
}


/*
 * Parse display string for multipart content
 * and use external program to display it.
 */

static int
show_multi_aux (CT ct, int serial, int alternate, char *cp)
{
    /* xstdin is only used in the call to parse_display_string():
       its value is ignored in the function. */
    int xstdin = 0, xlist = 0, xpause = 0, xtty = 0;
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

	    /* I'm not sure if this is necessary? */
	    p->c_storage = add (file, NULL);

	    if (p->c_showproc && !strcmp (p->c_showproc, "true"))
		return (alternate ? DONE : OK);
	    (*p->c_ceclosefnx) (p);
	}
    }

    if (parse_display_string (ct, cp, &xstdin, &xlist, &xpause, &xtty, file,
			      buffer, sizeof(buffer) - 1, 1)) {
	admonish (NULL, "Buffer overflow constructing show command!\n");
	return NOTOK;
    }

    return show_content_aux2 (ct, serial, alternate, NULL, buffer,
			      NOTOK, xlist, xpause, 0, xtty);
}


/*
 * show content of type "message/rfc822"
 */

static int
show_message_rfc822 (CT ct, int serial, int alternate)
{
    char *cp, buffer[BUFSIZ];
    CI ci = &ct->c_ctinfo;

    /* Check for invo_name-show-type/subtype */
    snprintf (buffer, sizeof(buffer), "%s-show-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, serial, alternate, cp, NULL);

    /* Check for invo_name-show-type */
    snprintf (buffer, sizeof(buffer), "%s-show-%s", invo_name, ci->ci_type);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, serial, alternate, cp, NULL);

    if ((cp = ct->c_showproc))
	return show_content_aux (ct, serial, alternate, cp, NULL);

    /* default method for message/rfc822 */
    if (ct->c_subtype == MESSAGE_RFC822) {
	cp = (ct->c_showproc = add ("%pshow -file '%F'", NULL));
	return show_content_aux (ct, serial, alternate, cp, NULL);
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
show_partial (CT ct, int serial, int alternate)
{
    NMH_UNUSED (serial);
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
show_external (CT ct, int serial, int alternate)
{
    struct exbody *e = (struct exbody *) ct->c_ctparams;
    CT p = e->eb_content;

    if (!type_ok (p, 0))
	return OK;

    return show_switch (p, serial, alternate);
}


static int
parse_display_string (CT ct, char *cp, int *xstdin, int *xlist, int *xpause,
                      int *xtty, char *file, char *buffer, size_t buflen,
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
		char **ap, **ep;
		char *s = "";

		for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++) {
		    snprintf (bp, buflen, "%s%s=\"%s\"", s, *ap, *ep);
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
		/* exclusive execution */
		*xtty = 1;
		break;

	    case 'F':
		/* %e, %f, and stdin is terminal not content */
		*xstdin = 1;
		*xtty = 1;
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

			/* Don't quote filename if it's already quoted.  Assume
			   it's quoted if previous character was a quote. */
			if (p->c_storage  &&  (*(p->c_storage-1) == '\'' ||
					       *(p->c_storage-1) == '"' ||
					       *(p->c_storage-1) == '`')) {
			    snprintf (bp, buflen, "%s%s", s, p->c_storage);
			} else {
			    snprintf (bp, buflen, "%s'%s'", s, p->c_storage);
			}

			len = strlen (bp);
			bp += len;
			buflen -= len;
			s = " ";
		    }
		} else {
		    /* insert filename containing content */
		    if (bp > buffer  &&
                        (*(bp-1) == '\'' || *(bp-1) == '"' || *(bp-1) == '`')) {
			/* Don't quote filename if it's already quoted.  Assume
			   it's quoted if previous character was a quote. */
			snprintf (bp, buflen, "%s", file);
		    } else {
			snprintf (bp, buflen, "'%s'", file);
		    }

		    /* since we've quoted the file argument, set things up
		     * to look past it, to avoid problems with the quoting
		     * logic below.  (I know, I should figure out what's
		     * broken with the quoting logic, but..)
		     */
		    len = strlen(bp);
		    bp += len;
		    buflen -= len;
		}

		/* set our starting pointer back to bp, to avoid
		 * requoting the filenames we just added
		 */
                pp = bp;
		break;

	    case 'p':
		/* %l, and pause prior to displaying content */
		*xpause = pausesw;
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
		static const char param[] = "charset";
		const char *value;
		const int found = parameter_value (ci, param, cp, &value);

		if (found == OK) {
		    /* Because it'll get incremented in the next iteration,
		       just increment by 1 for the '{'. */
		    cp += strlen(param) + 1;

		    /* cp pointed to the param and it's set in the
		       Content-Type header. */
		    strncpy (bp, value, buflen);

		    /* since we've quoted the file argument, set things up
		     * to look past it, to avoid problems with the quoting
		     * logic below.  (I know, I should figure out what's
		     * broken with the quoting logic, but..)
		     */
		    len = strlen(bp);
		    bp += len;
		    buflen -= len;
		    pp = bp;

		    break;
		} else if (found == 1) {
		    /* cp points to the param and it's not set in the
		       Content-Type header, so skip it. */
		    cp += strlen(param) + 1;

		    if (*cp == '\0') {
			break;
		    } else {
			if (*(cp + 1) == '\0') {
			    break;
			} else {
			    ++cp;
			    /* Increment cp again so that the last
			       character of the %{} token isn't output
			       after falling thru below. */
			    ++cp;
			}
		    }
		} else {
		    /* cp points to an unrecognized parameter.  Output
		       it as-is, starting here with the "%{". */
		    *bp++ = '%';
		    *bp++ = '{';
		    *bp = '\0';
		    buflen -= 2;
		    continue;
		}

		/* No parameter was found, so fall thru to default to
		   output the rest of the text as-is. */
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

	    /* Did we actually insert something? */
	    if (bp != pp) {
		/* Insert single quote if not inside quotes already */
		if (!quoted && buflen) {
		    len = strlen (pp);
		    memmove (pp + 1, pp, len);
		    *pp++ = '\'';
		    buflen--;
		    bp++;
		}
		/* Escape existing quotes */
		while ((pp = strchr (pp, '\'')) && buflen > 3) {
		    len = strlen (pp++);
		    memmove (pp + 3, pp, len);
		    *pp++ = '\\';
		    *pp++ = '\'';
		    *pp++ = '\'';
		    buflen -= 3;
		    bp += 3;
		}
		/* If pp is still set, that means we ran out of space. */
		if (pp)
		    buflen = 0;
		if (!quoted && buflen) {
		    *bp++ = '\'';
		    *bp = '\0';
		    buflen--;
		}
	    }
	} else {
raw:
	    *bp++ = *cp;
	    *bp = '\0';
	    buflen--;

	    if (*cp == '\'')
		quoted = !quoted;
	}
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
                char dest_buffer[BUFSIZ];
                ICONV_CONST char *ib = src_buffer;
                char *ob = dest_buffer;
                size_t outbytes = sizeof dest_buffer;
                size_t outbytes_before = outbytes;

                if (end > 0) bytes_to_read -= inbytes;

                if (iconv (conv_desc, &ib, &inbytes, &ob, &outbytes) ==
                    (size_t) -1) {
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

            /* Update ci_attrs. */
            src_charset = dest_charset;

            /* Update ct->c_ctline. */
            if (ct->c_ctline) {
                char *ctline =
                    update_attr (ct->c_ctline, "charset=", dest_charset);

                free (ct->c_ctline);
                ct->c_ctline = ctline;
            } /* else no CT line, which is odd */

            /* Update Content-Type header field. */
            for (hf = ct->c_first_hf; hf; hf = hf->next) {
                if (! strcasecmp (TYPE_FIELD, hf->name)) {
                    char *ctline_less_newline =
                        update_attr (hf->value, "charset=", dest_charset);
                    char *ctline = concat (ctline_less_newline, "\n", NULL);
                    free (ctline_less_newline);

                    free (hf->value);
                    hf->value = ctline;
                    break;
                }
            }
        } else {
            (void) m_unlink (dest);
        }
#else  /* ! HAVE_ICONV */
        NMH_UNUSED (message_mods);

        advise (NULL, "Can't convert %s to %s without iconv", src_charset,
                dest_charset);
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
 * Return values:
 *   NOTOK if cp doesn't point to {param}
 *   OK    if cp points to {param} and that attribute exists, and returns
 *         its value
 *   1     if cp points to {param} and that attribute doesn't exist
 */
int
parameter_value (CI ci, const char *param, const char *cp, const char **value) {
    int found = NOTOK;
    char *braced_param = concat ("{", param, "}", NULL);

    if (! strncasecmp(cp, braced_param, strlen (braced_param))) {
        char **ap, **ep;

        found = 1;

        for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ++ap, ++ep) {
            if (!strcasecmp (*ap, param)) {
                found = OK;
                *value = *ep;
                break;
            }
        }
    }

    free (braced_param);
    return found;
}


static void
intrser (int i)
{
    NMH_UNUSED (i);

    putchar ('\n');
    siglongjmp (intrenv, DONE);
}
