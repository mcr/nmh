
/*
 * mhshowsbr.c -- routines to display the contents of MIME messages
 *
 * $Id$
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/signals.h>
#include <h/md5.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <zotnet/mts/mts.h>
#include <zotnet/tws/tws.h>
#include <h/mime.h>
#include <h/mhparse.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

/*
 * Just use sigjmp/longjmp on older machines that
 * don't have sigsetjmp/siglongjmp.
 */
#ifndef HAVE_SIGSETJMP
# define sigjmp_buf jmp_buf
# define sigsetjmp(env,mask) setjmp(env)
# define siglongjmp(env,val) longjmp(env,val)
#endif

extern int errno;
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


/* termsbr.c */
int SOprintf (char *, ...);

/* mhparse.c */
int pidcheck (int);

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
static int show_content_aux2 (CT, int, int, char *, char *, int, int, int, int, int);
static int show_text (CT, int, int);
static int show_multi (CT, int, int);
static int show_multi_internal (CT, int, int);
static int show_multi_aux (CT, int, int, char *);
static int show_message_rfc822 (CT, int, int);
static int show_partial (CT, int, int);
static int show_external (CT, int, int);
static RETSIGTYPE intrser (int);


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
	if (type_ok (ct, 0))
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

#ifdef WAITINT
    int status;
#else
    union wait status;
#endif

    umask (ct->c_umask);

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
    SIGPROCMASK (SIG_BLOCK, &set, &oset);

    while (wait (&status) != NOTOK) {
#ifdef WAITINT
	pidcheck (status);
#else
	pidcheck (status.w_status);
#endif
	continue;
    }

    /* reset the signal mask */
    SIGPROCMASK (SIG_SETMASK, &oset, &set);

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
    char *vec[8];

    vecp = 0;
    vec[vecp++] = r1bindex (mhlproc, '/');
    vec[vecp++] = "-form";
    vec[vecp++] = form;
    vec[vecp++] = "-nobody";
    vec[vecp++] = ct->c_file;

    /*
     * If we've specified -(no)moreproc,
     * then just pass that along.
     */
    if (nomore) {
	vec[vecp++] = "-nomoreproc";
    } else if (progsw) {
	vec[vecp++] = "-moreproc";
	vec[vecp++] = progsw;
    }
    vec[vecp] = NULL;

    fflush (stdout);

    for (i = 0; (child_id = vfork()) == NOTOK && i < 5; i++)
	sleep (5);

    switch (child_id) {
    case NOTOK:
	adios ("fork", "unable to");
	/* NOTREACHED */

    case OK:
	execvp (mhlproc, vec);
	fprintf (stderr, "unable to exec ");
	perror (mhlproc);
	_exit (-1);
	/* NOTREACHED */

    default:
	xpid = -child_id;
	break;
    }
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
	    break;

	case CT_MESSAGE:
	    switch (ct->c_subtype) {
		case MESSAGE_PARTIAL:
		    return show_partial (ct, serial, alternate);
		    break;

		case MESSAGE_EXTERNAL:
		    return show_external (ct, serial, alternate);
		    break;

		case MESSAGE_RFC822:
		default:
		    return show_message_rfc822 (ct, serial, alternate);
		    break;
	    }
	    break;

	case CT_TEXT:
	    return show_text (ct, serial, alternate);
	    break;

	case CT_AUDIO:
	case CT_IMAGE:
	case CT_VIDEO:
	case CT_APPLICATION:
	    return show_content (ct, serial, alternate);
	    break;

	default:
	    adios (NULL, "unknown content type %d", ct->c_type);
	    break;
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

    /* Check for mhn-show-type/subtype */
    snprintf (buffer, sizeof(buffer), "%s-show-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, serial, alternate, cp, NULL);

    /* Check for mhn-show-type */
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
    int fd, len, buflen;
    int	xstdin, xlist, xpause, xtty;
    char *bp, *file, buffer[BUFSIZ];
    CI ci = &ct->c_ctinfo;

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
    
    xlist  = 0;
    xpause = 0;
    xstdin = 0;
    xtty   = 0;

    if (cracked) {
	strncpy (buffer, cp, sizeof(buffer));
	goto got_command;
    }

    /* get buffer ready to go */
    bp = buffer;
    bp[0] = '\0';
    buflen = sizeof(buffer);

    /* Now parse display string */
    for ( ; *cp; cp++) {
	if (*cp == '%') {
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
		xtty = 1;
		break;

	    case 'F':
		/* %e, %f, and stdin is terminal not content */
		xstdin = 1;
		xtty = 1;
		/* and fall... */

	    case 'f':
		/* insert filename containing content */
		snprintf (bp, buflen, "%s", file);
		break;

	    case 'p':
		/* %l, and pause prior to displaying content */
		xpause = pausesw;
		/* and fall... */

	    case 'l':
		/* display listing prior to displaying content */
		xlist = !nolist;
		break;

	    case 's':
		/* insert subtype of content */
		strncpy (bp, ci->ci_subtype, buflen);
		break;

	    case '%':
		/* insert character % */
		goto raw;

	    default:
		*bp++ = *--cp;
		*bp = '\0';
		buflen--;
		continue;
	    }
	    len = strlen (bp);
	    bp += len;
	    buflen -= len;
	} else {
raw:
	*bp++ = *cp;
	*bp = '\0';
	buflen--;
	}
    }

    /* use charset string to modify display method */
    if (ct->c_termproc) {
	char term[BUFSIZ];

	strncpy (term, buffer, sizeof(term));
	snprintf (buffer, sizeof(buffer), ct->c_termproc, term);
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
    char *vec[4], exec[BUFSIZ + sizeof "exec "];
    
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

	if (xpause && SOprintf ("Press <return> to show content..."))
	    printf ("Press <return> to show content...");

	if (xpause) {
	    int	intr;
	    SIGNAL_HANDLER istat;

	    istat = SIGNAL (SIGINT, intrser);
	    if ((intr = sigsetjmp (intrenv, 1)) == OK) {
		fflush (stdout);
		prompt[0] = 0;
		read (fileno (stdout), prompt, sizeof(prompt));
	    }
	    SIGNAL (SIGINT, istat);
	    if (intr != OK) {
		(*ct->c_ceclosefnx) (ct);
		return (alternate ? DONE : NOTOK);
	    }
	}
    }

    snprintf (exec, sizeof(exec), "exec %s", buffer);

    vec[0] = "/bin/sh";
    vec[1] = "-c";
    vec[2] = exec;
    vec[3] = NULL;

    fflush (stdout);

    for (i = 0; (child_id = vfork ()) == NOTOK && i < 5; i++)
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

    /* Check for mhn-show-type/subtype */
    snprintf (buffer, sizeof(buffer), "%s-show-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, serial, alternate, cp, NULL);

    /* Check for mhn-show-type */
    snprintf (buffer, sizeof(buffer), "%s-show-%s", invo_name, ci->ci_type);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, serial, alternate, cp, NULL);

    /*
     * Use default method if content is text/plain, or if
     * if it is not a text part of a multipart/alternative
     */
    if (!alternate || ct->c_subtype == TEXT_PLAIN) {
	snprintf (buffer, sizeof(buffer), "%%p%s '%%F'", progsw ? progsw :
		moreproc && *moreproc ? moreproc : "more");
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

    /* Check for mhn-show-type/subtype */
    snprintf (buffer, sizeof(buffer), "%s-show-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_multi_aux (ct, serial, alternate, cp);

    /* Check for mhn-show-type */
    snprintf (buffer, sizeof(buffer), "%s-show-%s", invo_name, ci->ci_type);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_multi_aux (ct, serial, alternate, cp);

    if ((cp = ct->c_showproc))
	return show_multi_aux (ct, serial, alternate, cp);

    /*
     * Use default method to display this multipart content
     * if it is not a (nested) part of a multipart/alternative,
     * or if it is one of the known subtypes of multipart.
     */
    if (!alternate || ct->c_subtype != MULTI_UNKNOWN)
	return show_multi_internal (ct, serial, alternate);

    return NOTOK;
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
	SIGPROCMASK (SIG_BLOCK, &set, &oset);
    }

/*
 * alternate   -> we are a part inside an multipart/alternative
 * alternating -> we are a multipart/alternative 
 */

    result = alternate ? NOTOK : OK;

    for (part = m->mp_parts; part; part = part->mp_next) {
	p = part->mp_part;

	if (part_ok (p, 0) && type_ok (p, 0)) {
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
#ifdef WAITINT
	int status;
#else
	union wait status;
#endif

	kids = 0;
	for (part = m->mp_parts; part; part = part->mp_next) {
	    p = part->mp_part;

	    if (p->c_pid > OK)
		if (kill (p->c_pid, 0) == NOTOK)
		    p->c_pid = 0;
		else
		    kids++;
	}

	while (kids > 0 && (pid = wait (&status)) != NOTOK) {
#ifdef WAITINT
	    pidcheck (status);
#else
	    pidcheck (status.w_status);
#endif

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
	SIGPROCMASK (SIG_SETMASK, &oset, &set);
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
    int len, buflen;
    int xlist, xpause, xtty;
    char *bp, *file, buffer[BUFSIZ];
    struct multipart *m = (struct multipart *) ct->c_ctparams;
    struct part *part;
    CI ci = &ct->c_ctinfo;
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

    xlist     = 0;
    xpause    = 0;
    xtty      = 0;

    /* get buffer ready to go */
    bp = buffer;
    bp[0] = '\0';
    buflen = sizeof(buffer);

    /* Now parse display string */
    for ( ; *cp; cp++) {
	if (*cp == '%') {
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
		xtty = 1;
		break;

	    case 'F':
		/* %e and %f */
		xtty = 1;
		/* and fall... */

	    case 'f':
		/* insert filename(s) containing content */
	    {
		char *s = "";
			
		for (part = m->mp_parts; part; part = part->mp_next) {
		    p = part->mp_part;

		    snprintf (bp, buflen, "%s'%s'", s, p->c_storage);
		    len = strlen (bp);
		    bp += len;
		    buflen -= len;
		    s = " ";
		}
	    }
	    break;

	    case 'p':
		/* %l, and pause prior to displaying content */
		xpause = pausesw;
		/* and fall... */

	    case 'l':
		/* display listing prior to displaying content */
		xlist = !nolist;
		break;

	    case 's':
		/* insert subtype of content */
		strncpy (bp, ci->ci_subtype, buflen);
		break;

	    case '%':
		/* insert character % */
		goto raw;

	    default:
		*bp++ = *--cp;
		*bp = '\0';
		buflen--;
		continue;
	    }
	    len = strlen (bp);
	    bp += len;
	    buflen -= len;
	} else {
raw:
	*bp++ = *cp;
	*bp = '\0';
	buflen--;
	}
    }

    /* use charset string to modify display method */
    if (ct->c_termproc) {
	char term[BUFSIZ];

	strncpy (term, buffer, sizeof(term));
	snprintf (buffer, sizeof(buffer), ct->c_termproc, term);
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

    /* Check for mhn-show-type/subtype */
    snprintf (buffer, sizeof(buffer), "%s-show-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
    if ((cp = context_find (buffer)) && *cp != '\0')
	return show_content_aux (ct, serial, alternate, cp, NULL);

    /* Check for mhn-show-type */
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

#if 0
    content_error (NULL, p, "don't know how to display content");
    return NOTOK;
#endif
}


static RETSIGTYPE
intrser (int i)
{
#ifndef RELIABLE_SIGNALS
    SIGNAL (SIGINT, intrser);
#endif

    putchar ('\n');
    siglongjmp (intrenv, DONE);
}
