
/*
 * sendmail.c -- nmh sendmail interface
 *
 * $Id$
 */

#include <h/mh.h>
#include <mts/smtp/smtp.h>
#include <zotnet/mts/mts.h>
#include <signal.h>
#include "h/signals.h"  /* for SIGNAL() */
#ifdef MPOP
#include <errno.h>
#endif

/*
 * This module implements an interface to SendMail very similar
 * to the MMDF mm_(3) routines.  The sm_() routines herein talk
 * SMTP to a sendmail process, mapping SMTP reply codes into
 * RP_-style codes.
 */

/*
 * On older 4.2BSD machines without the POSIX function `sigaction',
 * the alarm handing stuff for time-outs will NOT work due to the way
 * syscalls get restarted.  This is not really crucial, since SendMail
 * is generally well-behaved in this area.
 */

#ifdef SENDMAILBUG
/*
 * It appears that some versions of Sendmail will return Code 451
 * when they don't really want to indicate a failure.
 * "Code 451 almost always means sendmail has deferred; we don't
 * really want bomb out at this point since sendmail will rectify
 * things later."  So, if you define SENDMAILBUG, Code 451 is
 * considered the same as Code 250.  Yuck!
 */
#endif

#define	TRUE	1
#define	FALSE	0

#define	NBITS ((sizeof (int)) * 8)

/*
 * these codes must all be different!
 */
#define	SM_OPEN	 90      /* Changed from 30 in case of nameserver flakiness */
#define	SM_HELO	 20
#define	SM_RSET	 15
#define	SM_MAIL	 40
#define	SM_RCPT	120
#define	SM_DATA	 20
#define	SM_TEXT	150
#define	SM_DOT	180
#define	SM_QUIT	 30
#define	SM_CLOS	 10

static int sm_addrs = 0;
static int sm_alarmed = 0;
static int sm_child = NOTOK;
static int sm_debug = 0;
static int sm_nl = TRUE;
static int sm_verbose = 0;

static FILE *sm_rfp = NULL;
static FILE *sm_wfp = NULL;

#ifdef MPOP
static int sm_ispool = 0;
static char sm_tmpfil[BUFSIZ];
#endif /* MPOP */

static char *sm_noreply = "No reply text given";
static char *sm_moreply = "; ";

struct smtp sm_reply;		/* global... */

static int doingEHLO;

#define	MAXEHLO	20
char *EHLOkeys[MAXEHLO + 1];

/*
 * static prototypes
 */
static int sm_ierror (char *fmt, ...);
static int smtalk (int time, char *fmt, ...);
static int sm_wrecord (char *, int);
static int sm_wstream (char *, int);
static int sm_werror (void);
static int smhear (void);
static int sm_rrecord (char *, int *);
static int sm_rerror (void);
static RETSIGTYPE alrmser (int);


int
sm_init (char *client, char *server, int watch, int verbose,
         int debug, int onex, int queued, int sasl, char *saslmech, char *user)
{
    int i, result, vecp;
    int pdi[2], pdo[2];
    char *vec[15];

    if (watch)
	verbose = TRUE;

    sm_verbose = verbose;
    sm_debug = debug;
    if (sm_rfp != NULL && sm_wfp != NULL)
	return RP_OK;

    if (client == NULL || *client == '\0') {
	if (clientname)
	    client = clientname;
	else
	    client = LocalName();	/* no clientname -> LocalName */
    }

    if (sasl)
	return sm_ierror("SASL authentication not supported with the "
			 "Sendmail MTA");

#ifdef ZMAILER
    if (client == NULL || *client == '\0')
	client = "localhost";
#endif

    if (pipe (pdi) == NOTOK)
	return sm_ierror ("no pipes");
    if (pipe (pdo) == NOTOK) {
	close (pdi[0]);
	close (pdi[1]);
	return sm_ierror ("no pipes");
    }

    for (i = 0; (sm_child = fork ()) == NOTOK && i < 5; i++)
	sleep (5);

    switch (sm_child) {
	case NOTOK: 
	    close (pdo[0]);
	    close (pdo[1]);
	    close (pdi[0]);
	    close (pdi[1]);
	    return sm_ierror ("unable to fork");

	case OK: 
	    if (pdo[0] != fileno (stdin))
		dup2 (pdo[0], fileno (stdin));
	    if (pdi[1] != fileno (stdout))
		dup2 (pdi[1], fileno (stdout));
	    if (pdi[1] != fileno (stderr))
		dup2 (pdi[1], fileno (stderr));
	    for (i = fileno (stderr) + 1; i < NBITS; i++)
		close (i);

	    vecp = 0;
	    vec[vecp++] = r1bindex (sendmail, '/');
	    vec[vecp++] = "-bs";
#ifndef ZMAILER
	    vec[vecp++] = watch ? "-odi" : queued ? "-odq" : "-odb";
	    vec[vecp++] = "-oem";
	    vec[vecp++] = "-om";
# ifndef RAND
	    if (verbose)
		vec[vecp++] = "-ov";
# endif /* not RAND */
#endif /* not ZMAILER */
	    vec[vecp++] = NULL;

	    setgid (getegid ());
	    setuid (geteuid ());
	    execvp (sendmail, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (sendmail);
	    _exit (-1);		/* NOTREACHED */

	default: 
	    SIGNAL (SIGALRM, alrmser);
	    SIGNAL (SIGPIPE, SIG_IGN);

	    close (pdi[1]);
	    close (pdo[0]);
	    if ((sm_rfp = fdopen (pdi[0], "r")) == NULL
		    || (sm_wfp = fdopen (pdo[1], "w")) == NULL) {
		close (pdi[0]);
		close (pdo[1]);
		sm_rfp = sm_wfp = NULL;
		return sm_ierror ("unable to fdopen");
	    }
	    sm_alarmed = 0;
	    alarm (SM_OPEN);
	    result = smhear ();
	    alarm (0);
	    switch (result) {
		case 220: 
		    break;

		default: 
		    sm_end (NOTOK);
		    return RP_RPLY;
	    }

	    if (client && *client) {
		doingEHLO = 1;
		result = smtalk (SM_HELO, "EHLO %s", client);
		doingEHLO = 0;

		if (500 <= result && result <= 599)
		    result = smtalk (SM_HELO, "HELO %s", client);

		switch (result) {
		    case 250:
		        break;

		    default:
			sm_end (NOTOK);
			return RP_RPLY;
		}
	    }

#ifndef ZMAILER
	    if (onex)
		smtalk (SM_HELO, "ONEX");
#endif
	    if (watch)
		smtalk (SM_HELO, "VERB on");

	    return RP_OK;
    }
}


int
sm_winit (int mode, char *from)
{
#ifdef MPOP
    if (sm_ispool && !sm_wfp) {
	strlen (strcpy (sm_reply.text, "unable to create new spool file"));
	sm_reply.code = NOTOK;
	return RP_BHST;
    }
#endif /* MPOP */

    switch (smtalk (SM_MAIL, "%s FROM:<%s>",
		mode == S_SEND ? "SEND" : mode == S_SOML ? "SOML"
		: mode == S_SAML ? "SAML" : "MAIL", from)) {
	case 250: 
	    sm_addrs = 0;
	    return RP_OK;

	case 500: 
	case 501: 
	case 552: 
	    return RP_PARM;

	default: 
	    return RP_RPLY;
    }
}


int
sm_wadr (char *mbox, char *host, char *path)
{
    switch (smtalk (SM_RCPT, host && *host ? "RCPT TO:<%s%s@%s>"
					   : "RCPT TO:<%s%s>",
			     path ? path : "", mbox, host)) {
	case 250: 
	case 251: 
	    sm_addrs++;
	    return RP_OK;

	case 451: 
#ifdef SENDMAILBUG
	    sm_addrs++;
	    return RP_OK;
#endif /* SENDMAILBUG */
	case 421: 
	case 450: 
	case 452: 
	    return RP_NO;

	case 500: 
	case 501: 
	    return RP_PARM;

	case 550: 
	case 551: 
	case 552: 
	case 553: 
	    return RP_USER;

	default: 
	    return RP_RPLY;
    }
}


int
sm_waend (void)
{
    switch (smtalk (SM_DATA, "DATA")) {
	case 354: 
	    sm_nl = TRUE;
	    return RP_OK;

	case 451: 
#ifdef SENDMAILBUG
	    sm_nl = TRUE;
	    return RP_OK;
#endif /* SENDMAILBUG */
	case 421: 
	    return RP_NO;

	case 500: 
	case 501: 
	case 503: 
	case 554: 
	    return RP_NDEL;

	default: 
	    return RP_RPLY;
    }
}


int
sm_wtxt (char *buffer, int len)
{
    int result;

    sm_alarmed = 0;
    alarm (SM_TEXT);
    result = sm_wstream (buffer, len);
    alarm (0);

    return (result == NOTOK ? RP_BHST : RP_OK);
}


int
sm_wtend (void)
{
    if (sm_wstream ((char *) NULL, 0) == NOTOK)
	return RP_BHST;

    switch (smtalk (SM_DOT + 3 * sm_addrs, ".")) {
	case 250: 
	case 251: 
	    return RP_OK;

	case 451: 
#ifdef SENDMAILBUG
	    return RP_OK;
#endif /* SENDMAILBUG */
	case 452: 
	default: 
	    return RP_NO;

	case 552: 
	case 554: 
	    return RP_NDEL;
    }
}


int
sm_end (int type)
{
    int status;
    struct smtp sm_note;

    switch (sm_child) {
	case NOTOK: 
	case OK: 
	    return RP_OK;

	default: 
	    break;
    }

    if (sm_rfp == NULL && sm_wfp == NULL)
	return RP_OK;

    switch (type) {
	case OK: 
	    smtalk (SM_QUIT, "QUIT");
	    break;

	case NOTOK: 
	    sm_note.code = sm_reply.code;
	    strncpy (sm_note.text, sm_reply.text, sm_note.length = sm_reply.length);/* fall */
	case DONE: 
	    if (smtalk (SM_RSET, "RSET") == 250 && type == DONE)
		return RP_OK;
	    kill (sm_child, SIGKILL);
	    discard (sm_rfp);
	    discard (sm_wfp);
	    if (type == NOTOK) {
		sm_reply.code = sm_note.code;
		strncpy (sm_reply.text, sm_note.text, sm_reply.length = sm_note.length);
	    }
	    break;
    }
    if (sm_rfp != NULL) {
	alarm (SM_CLOS);
	fclose (sm_rfp);
	alarm (0);
    }
    if (sm_wfp != NULL) {
	alarm (SM_CLOS);
	fclose (sm_wfp);
	alarm (0);
    }

    status = pidwait (sm_child, OK);

    sm_child = NOTOK;
    sm_rfp = sm_wfp = NULL;

    return (status ? RP_BHST : RP_OK);
}


static int
sm_ierror (char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf (sm_reply.text, sizeof(sm_reply.text), fmt, ap);
    va_end(ap);

    sm_reply.length = strlen (sm_reply.text);
    sm_reply.code = NOTOK;

    return RP_BHST;
}


static int
smtalk (int time, char *fmt, ...)
{
    int result;
    char buffer[BUFSIZ];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf (buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    if (sm_debug) {
	printf ("=> %s\n", buffer);
	fflush (stdout);
    }

#ifdef MPOP
    if (sm_ispool) {
	char file[BUFSIZ];

	if (strcmp (buffer, ".") == 0)
	    time = SM_DOT;
	fprintf (sm_wfp, "%s\r\n", buffer);
	switch (time) {
	    case SM_DOT:
	        fflush (sm_wfp);
	        if (ferror (sm_wfp))
		    return sm_werror ();
		snprintf (file, sizeof(file), "%s%c.bulk", sm_tmpfil,
				(char) (sm_ispool + 'a' - 1));
		if (rename (sm_tmpfil, file) == NOTOK) {
		    int len;
		    char *bp;

		    snprintf (sm_reply.text, sizeof(sm_reply.text),
			"error renaming %s to %s: ", sm_tmpfil, file);
		    bp = sm_reply.text;
		    len = strlen (bp);
		    bp += len;
		    if ((s = strerror (errno)))
			strncpy (bp, s, sizeof(sm_reply.text) - len);
		    else
			snprintf (bp, sizeof(sm_reply.text) - len,
				"unknown error %d", errno);
		    sm_reply.length = strlen (sm_reply.text);
		    sm_reply.code = NOTOK;
		    return RP_BHST;
		}
		fclose (sm_wfp);
		if (sm_wfp = fopen (sm_tmpfil, "w"))
		    chmod (sm_tmpfil, 0600);
		sm_ispool++;
		/* and fall... */

	    case SM_MAIL:
	    case SM_RCPT:
	        result = 250;
		break;

	    case SM_RSET:
		fflush (sm_wfp);
		ftruncate (fileno (sm_wfp), 0L);
		fseek (sm_wfp, 0L, SEEK_SET);
	        result = 250;
		break;

	    case SM_DATA:
	        result = 354;
		break;

	    case SM_QUIT:
		unlink (sm_tmpfil);
		sm_ispool = 0;
	        result = 221;
		break;

	    default:
		result = 500;
		break;
	}
	if (sm_debug) {
	    printf ("<= %d\n", result);
	    fflush (stdout);
	}

	sm_reply.text[sm_reply.length = 0] = NULL;
	return (sm_reply.code = result);
    }
#endif /* MPOP */

    sm_alarmed = 0;
    alarm ((unsigned) time);
    if ((result = sm_wrecord (buffer, strlen (buffer))) != NOTOK)
	result = smhear ();
    alarm (0);

    return result;
}


static int
sm_wrecord (char *buffer, int len)
{
    if (sm_wfp == NULL)
	return sm_werror ();

    fwrite (buffer, sizeof *buffer, len, sm_wfp);
    fputs ("\r\n", sm_wfp);
    fflush (sm_wfp);

    return (ferror (sm_wfp) ? sm_werror () : OK);
}


static int
sm_wstream (char *buffer, int len)
{
    char *bp;
    static char lc = 0;

    if (sm_wfp == NULL)
	return sm_werror ();

    if (buffer == NULL && len == 0) {
	if (lc != '\n')
	    fputs ("\r\n", sm_wfp);
	lc = 0;
	return (ferror (sm_wfp) ? sm_werror () : OK);
    }

    for (bp = buffer; len > 0; bp++, len--) {
	switch (*bp) {
	    case '\n': 
		sm_nl = TRUE;
		fputc ('\r', sm_wfp);
		break;

	    case '.': 
		if (sm_nl)
		    fputc ('.', sm_wfp);/* FALL THROUGH */
	    default: 
		sm_nl = FALSE;
	}
	fputc (*bp, sm_wfp);
	if (ferror (sm_wfp))
	    return sm_werror ();
    }

    if (bp > buffer)
	lc = *--bp;
    return (ferror (sm_wfp) ? sm_werror () : OK);
}


#ifdef _AIX
/*
 * AIX by default will inline the strlen and strcpy commands by redefining
 * them as __strlen and __strcpy respectively.  This causes compile problems
 * with the #ifdef MPOP in the middle.  Should the #ifdef MPOP be removed,
 * remove these #undefs.
 */
# undef strlen
# undef strcpy
#endif /* _AIX */

static int
sm_werror (void)
{
    sm_reply.length =
	strlen (strcpy (sm_reply.text, sm_wfp == NULL ? "no pipe opened"
	    : sm_alarmed ? "write to pipe timed out"
	    : "error writing to pipe"));

    return (sm_reply.code = NOTOK);
}


static int
smhear (void)
{
    int i, code, cont, bc, rc, more;
    char *bp, *rp;
    char **ehlo, buffer[BUFSIZ];

    if (doingEHLO) {
	static int at_least_once = 0;

	if (at_least_once) {
	    for (ehlo = EHLOkeys; *ehlo; ehlo++)
		free (*ehlo);
	} else {
	    at_least_once = 1;
	}

	*(ehlo = EHLOkeys) = NULL;
    }

again:

    sm_reply.text[sm_reply.length = 0] = 0;

    rp = sm_reply.text;
    rc = sizeof(sm_reply.text) - 1;

    for (more = FALSE; sm_rrecord (bp = buffer, &bc) != NOTOK;) {
	if (sm_debug) {
	    printf ("<= %s\n", buffer);
	    fflush (stdout);
	}

	if (doingEHLO
	        && strncmp (buffer, "250", sizeof("250") - 1) == 0
	        && (buffer[3] == '-' || doingEHLO == 2)
	        && buffer[4]) {
	    if (doingEHLO == 2) {
		if ((*ehlo = malloc ((size_t) (strlen (buffer + 4) + 1)))) {
		    strcpy (*ehlo++, buffer + 4);
		    *ehlo = NULL;
		    if (ehlo >= EHLOkeys + MAXEHLO)
			doingEHLO = 0;
		}
		else
		    doingEHLO = 0;
	    }
	    else
		doingEHLO = 2;
	}

	for (; bc > 0 && (!isascii (*bp) || !isdigit (*bp)); bp++, bc--)
	    continue;

	cont = FALSE;
	code = atoi (bp);
	bp += 3, bc -= 3;
	for (; bc > 0 && isspace (*bp); bp++, bc--)
	    continue;
	if (bc > 0 && *bp == '-') {
	    cont = TRUE;
	    bp++, bc--;
	    for (; bc > 0 && isspace (*bp); bp++, bc--)
		continue;
	}

	if (more) {
	    if (code != sm_reply.code || cont)
		continue;
	    more = FALSE;
	} else {
	    sm_reply.code = code;
	    more = cont;
	    if (bc <= 0) {
		strncpy (buffer, sm_noreply, sizeof(buffer));
		bp = buffer;
		bc = strlen (sm_noreply);
	    }
	}
	if ((i = min (bc, rc)) > 0) {
	    strncpy (rp, bp, i);
	    rp += i;
	    rc -= i;
	    if (more && rc > strlen (sm_moreply) + 1) {
		strncpy (sm_reply.text + rc, sm_moreply, sizeof(sm_reply.text) - rc);
		rc += strlen (sm_moreply);
	    }
	}
	if (more)
	    continue;
	if (sm_reply.code < 100) {
	    if (sm_verbose) {
		printf ("%s\n", sm_reply.text);
		fflush (stdout);
	    }
	    goto again;
	}

	sm_reply.length = rp - sm_reply.text;

	return sm_reply.code;
    }

    return NOTOK;
}


static int
sm_rrecord (char *buffer, int *len)
{
    if (sm_rfp == NULL)
	return sm_rerror ();

    buffer[*len = 0] = 0;

    fgets (buffer, BUFSIZ, sm_rfp);
    *len = strlen (buffer);
    if (ferror (sm_rfp) || feof (sm_rfp))
	return sm_rerror ();
    if (buffer[*len - 1] != '\n')
	while (getc (sm_rfp) != '\n' && !ferror (sm_rfp) && !feof (sm_rfp))
	    continue;
    else
	if (buffer[*len - 2] == '\r')
	    *len -= 1;
    buffer[*len - 1] = 0;

    return OK;
}


static int
sm_rerror (void)
{
    sm_reply.length =
	strlen (strcpy (sm_reply.text, sm_rfp == NULL ? "no pipe opened"
	    : sm_alarmed ? "read from pipe timed out"
	    : feof (sm_rfp) ? "premature end-of-file on pipe"
	    : "error reading from pipe"));

    return (sm_reply.code = NOTOK);
}


static RETSIGTYPE
alrmser (int i)
{
#ifndef	RELIABLE_SIGNALS
    SIGNAL (SIGALRM, alrmser);
#endif

    sm_alarmed++;
    if (sm_debug) {
	printf ("timed out...\n");
	fflush (stdout);
    }
}


char *
rp_string (int code)
{
    char  *text;
    static char buffer[BUFSIZ];

    switch (sm_reply.code != NOTOK ? code : NOTOK) {
	case RP_AOK:
	    text = "AOK";
	    break;

	case RP_MOK:
	    text = "MOK";
	    break;

	case RP_OK: 
	    text = "OK";
	    break;

	case RP_RPLY: 
	    text = "RPLY";
	    break;

	case RP_BHST: 
	default: 
	    text = "BHST";
	    snprintf (buffer, sizeof(buffer), "[%s] %s", text, sm_reply.text);
	    return buffer;

	case RP_PARM: 
	    text = "PARM";
	    break;

	case RP_NO: 
	    text = "NO";
	    break;

	case RP_USER: 
	    text = "USER";
	    break;

	case RP_NDEL: 
	    text = "NDEL";
	    break;
    }

    snprintf (buffer, sizeof(buffer), "[%s] %3d %s",
		text, sm_reply.code, sm_reply.text);
    return buffer;
}

