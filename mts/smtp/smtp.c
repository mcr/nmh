/*
 * smtp.c -- nmh SMTP interface
 *
 * $Id$
 */

#include <h/mh.h>
#include "smtp.h"
#include <zotnet/mts/mts.h>
#include <signal.h>
#include <h/signals.h>
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
#define	SM_AUTH  45

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

#ifdef CYRUS_SASL
/*
 * Some globals needed by SASL
 */

static sasl_conn_t *conn = NULL;	/* SASL connection state */
static int sasl_complete = 0;		/* Has authentication succeded? */
static sasl_ssf_t sasl_ssf;		/* Our security strength factor */
static char *sasl_pw_context[2];	/* Context to pass into sm_get_pass */
static int maxoutbuf;			/* Maximum crypto output buffer */
static int sm_get_user(void *, int, const char **, unsigned *);
static int sm_get_pass(sasl_conn_t *, void *, int, sasl_secret_t **);

static sasl_callback_t callbacks[] = {
    { SASL_CB_USER, sm_get_user, NULL },
#define SM_SASL_N_CB_USER 0
    { SASL_CB_PASS, sm_get_pass, NULL },
#define SM_SASL_N_CB_PASS 1
    { SASL_CB_LIST_END, NULL, NULL },
};
#endif /* CYRUS_SASL */

static char *sm_noreply = "No reply text given";
static char *sm_moreply = "; ";

struct smtp sm_reply;		/* global... */

#define	MAXEHLO	20

static int doingEHLO;
char *EHLOkeys[MAXEHLO + 1];

/*
 * static prototypes
 */
static int smtp_init (char *, char *, int, int, int, int, int, int,
		      char *, char *);
static int sendmail_init (char *, char *, int, int, int, int, int);

static int rclient (char *, char *, char *);
static int sm_ierror (char *fmt, ...);
static int smtalk (int time, char *fmt, ...);
static int sm_wrecord (char *, int);
static int sm_wstream (char *, int);
static int sm_werror (void);
static int smhear (void);
static int sm_rrecord (char *, int *);
static int sm_rerror (void);
static RETSIGTYPE alrmser (int);
static char *EHLOset (char *);

#ifdef MPOP
/*
 * smtp.c's own static copy of several nmh library subroutines
 */
static char **smail_brkstring (char *, char *, char *);
static int smail_brkany (char, char *);
char **smail_copyip (char **, char **, int);
#endif

#ifdef CYRUS_SASL
/*
 * Function prototypes needed for SASL
 */

static int sm_auth_sasl(char *, char *, char *);
#endif /* CYRUS_SASL */

/* from zotnet/mts/client.c */
int client (char *, char *, char *, int, char *, int);

int
sm_init (char *client, char *server, int watch, int verbose,
         int debug, int onex, int queued, int sasl, char *saslmech,
         char *user)
{
    if (sm_mts == MTS_SMTP)
	return smtp_init (client, server, watch, verbose,
			  debug, onex, queued, sasl, saslmech, user);
    else
	return sendmail_init (client, server, watch, verbose,
			      debug, onex, queued);
}

static int
smtp_init (char *client, char *server, int watch, int verbose,
	   int debug, int onex, int queued, int sasl, char *saslmech,
	   char *user)
{
#ifdef CYRUS_SASL
    char *server_mechs;
#endif /* CYRUS_SASL */
    int result, sd1, sd2;

    if (watch)
	verbose = TRUE;

    sm_verbose = verbose;
    sm_debug = debug;

#ifdef MPOP
    if (sm_ispool)
	goto all_done;
#endif

    if (sm_rfp != NULL && sm_wfp != NULL)
	goto send_options;

    if (client == NULL || *client == '\0') {
	if (clientname) {
	    client = clientname;
	} else {
	    client = LocalName();	/* no clientname -> LocalName */
	}
    }

#ifdef ZMAILER
    if (client == NULL || *client == '\0')
	client = "localhost";
#endif

    if ((sd1 = rclient (server, "tcp", "smtp")) == NOTOK)
	return RP_BHST;

#ifdef MPOP
    if (sm_ispool) {
	if (sm_rfp) {
	    alarm (SM_CLOS);
	    fclose (sm_rfp);
	    alarm (0);
	    sm_rfp = NULL;
	}
	if ((sm_wfp = fdopen (sd1, "w")) == NULL) {
	    unlink (sm_tmpfil);
	    close (sd1);
	    return sm_ierror ("unable to fdopen");
	}
all_done: ;
	sm_reply.text[sm_reply.length = 0] = NULL;
	return (sm_reply.code = RP_OK);
    }
#endif /* MPOP */

    if ((sd2 = dup (sd1)) == NOTOK) {
	close (sd1);
	return sm_ierror ("unable to dup");
    }

    SIGNAL (SIGALRM, alrmser);
    SIGNAL (SIGPIPE, SIG_IGN);

    if ((sm_rfp = fdopen (sd1, "r")) == NULL
	    || (sm_wfp = fdopen (sd2, "w")) == NULL) {
	close (sd1);
	close (sd2);
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

    /*
     * Give EHLO or HELO command
     */
    if (client && *client) {
	doingEHLO = 1;
	result = smtalk (SM_HELO, "EHLO %s", client);
	doingEHLO = 0;

	if (result >= 500 && result <= 599)
	    result = smtalk (SM_HELO, "HELO %s", client);

	if (result != 250) {
	    sm_end (NOTOK);
	    return RP_RPLY;
	}
    }

#ifdef CYRUS_SASL
    /*
     * If the user asked for SASL, then check to see if the SMTP server
     * supports it.  Otherwise, error out (because the SMTP server
     * might have been spoofed; we don't want to just silently not
     * do authentication
     */

    if (sasl) {
	if (! (server_mechs = EHLOset("AUTH"))) {
	    sm_end(NOTOK);
	    return sm_ierror("SMTP server does not support SASL");
	}

	if (saslmech && stringdex(saslmech, server_mechs) == -1) {
	    sm_end(NOTOK);
	    return sm_ierror("Requested SASL mech \"%s\" is not in the "
			     "list of supported mechanisms:\n%s",
			     saslmech, server_mechs);
	}

	if (sm_auth_sasl(user, saslmech ? saslmech : server_mechs,
			 server) != RP_OK) {
	    sm_end(NOTOK);
	    return NOTOK;
	}
    }
#endif /* CYRUS_SASL */

send_options: ;
    if (watch && EHLOset ("XVRB"))
	smtalk (SM_HELO, "VERB on");
    if (onex && EHLOset ("XONE"))
	smtalk (SM_HELO, "ONEX");
    if (queued && EHLOset ("XQUE"))
	smtalk (SM_HELO, "QUED");

    return RP_OK;
}

int
sendmail_init (char *client, char *server, int watch, int verbose,
               int debug, int onex, int queued)
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

#ifdef MPOP
# define MAXARGS  1000
#endif /* MPOP */

static int
rclient (char *server, char *protocol, char *service)
{
    int sd;
    char response[BUFSIZ];
#ifdef MPOP
    char *cp;
#endif /* MPOP */

    if ((sd = client (server, protocol, service, FALSE, response, sizeof(response))) != NOTOK)
	return sd;

#ifdef MPOP
    if (!server && servers && (cp = strchr(servers, '/'))) {
	char **ap;
	char *arguments[MAXARGS];

	smail_copyip (smail_brkstring (cp = getcpy (servers), " ", "\n"), arguments, MAXARGS);

	for (ap = arguments; *ap; ap++)
	    if (**ap == '/') {
		char *dp;

		if ((dp = strrchr(*ap, '/')) && *++dp == NULL)
		    *--dp = NULL;
		snprintf (sm_tmpfil, sizeof(sm_tmpfil), "%s/smtpXXXXXX", *ap);
#ifdef HAVE_MKSTEMP
		sd = mkstemp (sm_tmpfil);
#else
		mktemp (sm_tmpfil);

		if ((sd = creat (sm_tmpfil, 0600)) != NOTOK) {
		    sm_ispool = 1;
		    break;
		}
#endif
	    }

	free (cp);
	if (sd != NOTOK)
	    return sd;
    }
#endif /* MPOP */

    sm_ierror ("%s", response);
    return NOTOK;
}

#ifdef CYRUS_SASL
#include <sasl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#endif /* CYRUS_SASL */

int
sm_winit (int mode, char *from)
{
    char *smtpcom;

#ifdef MPOP
    if (sm_ispool && !sm_wfp) {
	strlen (strcpy (sm_reply.text, "unable to create new spool file"));
	sm_reply.code = NOTOK;
	return RP_BHST;
    }
#endif /* MPOP */

    switch (mode) {
	case S_MAIL:
	    smtpcom = "MAIL";
	    break;

	case S_SEND:
	    smtpcom = "SEND";
	    break;

	case S_SOML:
	    smtpcom = "SOML";
	    break;

	case S_SAML:
	    smtpcom = "SAML";
	    break;
    }

    switch (smtalk (SM_MAIL, "%s FROM:<%s>", smtpcom, from)) {
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

    if (sm_mts == MTS_SENDMAIL) {
	switch (sm_child) {
	    case NOTOK: 
	    case OK: 
		return RP_OK;

	    default: 
		break;
	}
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
	    if (sm_mts == MTS_SMTP)
		smtalk (SM_QUIT, "QUIT");
	    else {
		kill (sm_child, SIGKILL);
		discard (sm_rfp);
		discard (sm_wfp);
	    }
	    if (type == NOTOK) {
		sm_reply.code = sm_note.code;
		strncpy (sm_reply.text, sm_note.text, sm_reply.length = sm_note.length);
	    }
	    break;
    }

#ifdef MPOP
    if (sm_ispool) {
	sm_ispool = 0;

	if (sm_wfp) {
	    unlink (sm_tmpfil);
	    fclose (sm_wfp);
	    sm_wfp = NULL;
	}
    }
#endif /* MPOP */

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

    if (sm_mts == MTS_SMTP) {
	status = 0;
#ifdef CYRUS_SASL
	if (conn)
	    sasl_dispose(&conn);
#endif /* CYRUS_SASL */
    } else {
	status = pidwait (sm_child, OK);
	sm_child = NOTOK;
    }

    sm_rfp = sm_wfp = NULL;
    return (status ? RP_BHST : RP_OK);
}


#ifdef MPOP

int
sm_bulk (char *file)
{
    int	cc, i, j, k, result;
    long pos;
    char *dp, *bp, *cp, s;
    char buffer[BUFSIZ], sender[BUFSIZ];
    FILE *fp, *gp;

    gp = NULL;
    k = strlen (file) - sizeof(".bulk");
    if ((fp = fopen (file, "r")) == NULL) {
	int len;

	snprintf (sm_reply.text, sizeof(sm_reply.text),
		"unable to read %s: ", file);
	bp = sm_reply.text;
	len = strlen (bp);
	bp += len;
	if ((s = strerror (errno)))
	    strncpy (bp, s, sizeof(sm_reply.text) - len);
	else
	    snprintf (bp, sizeof(sm_reply.text) - len, "Error %d", errno);
	sm_reply.length = strlen (sm_reply.text);
	sm_reply.code = NOTOK;
	return RP_BHST;
    }
    if (sm_debug) {
	printf ("reading file %s\n", file);
	fflush (stdout);
    }

    i = j = 0;
    while (fgets (buffer, sizeof(buffer), fp)) {
	if (j++ == 0)
	    strncpy (sender, buffer + sizeof("MAIL FROM:") - 1, sizeof(sender));
	if (strcmp (buffer, "DATA\r\n") == 0) {
	    i = 1;
	    break;
	}
    }
    if (i == 0) {
	if (sm_debug) {
	    printf ("no DATA...\n");
	    fflush (stdout);
	}
losing0:
	snprintf (buffer, sizeof(buffer), "%s.bad", file);
	rename (file, buffer);
	if (gp) {
	    snprintf (buffer, sizeof(buffer), "%*.*sA.bulk", k, k, file);
	    unlink (buffer);
	    fclose (gp);
	}
	fclose (fp);
	return RP_OK;
    }
    if (j < 3) {
	if (sm_debug) {
	    printf ("no %srecipients...\n", j < 1 ? "sender or " : "");
	    fflush (stdout);
	}
	goto losing0;
    }

    if ((cp = malloc ((size_t) (cc = (pos = ftell (fp)) + 1))) == NULL) {
	sm_reply.length = strlen (strcpy (sm_reply.text, "out of memory"));
losing1: ;
	sm_reply.code = NOTOK;
	fclose (fp);
	return RP_BHST;
    }
    fseek (fp, 0L, SEEK_SET);
    for (dp = cp, i = 0; i++ < j; dp += strlen (dp))
	if (fgets (dp, cc - (dp - cp), fp) == NULL) {
	    sm_reply.length = strlen (strcpy (sm_reply.text, "premature eof"));
losing2:
	    free (cp);
	    goto losing1;
	}
    *dp = NULL;

    for (dp = cp, i = cc - 1; i > 0; dp += cc, i -= cc)
	if ((cc = write (fileno (sm_wfp), dp, i)) == NOTOK) {
	    int len;
losing3:
	    strcpy (sm_reply.text, "error writing to server: ",
		sizeof(sm_reply.text));
	    bp = sm_reply.text;
	    len = strlen (bp);
	    bp += len;
	    if ((s = strerror (errno)))
		strncpy (bp, s, sizeof(sm_reply.text) - len);
	    else
		snprintf (bp, sizeof(sm_reply.text) - len,
			"unknown error %d", errno);
	    sm_reply.length = strlen (sm_reply.text);
	    goto losing2;
	}
	else
	    if (sm_debug) {
		printf ("wrote %d octets to server\n", cc);
		fflush (stdout);
	    }

    for (dp = cp, i = 0; i++ < j; dp = strchr(dp, '\n'), dp++) {
	if (sm_debug) {
	    if (bp = strchr(dp, '\r'))
		*bp = NULL;
	    printf ("=> %s\n", dp);
	    fflush (stdout);
	    if (bp)
		*bp = '\r';
	}

	switch (smhear () + (i == 1 ? 1000 : i != j ? 2000 : 3000)) {
	    case 1000 + 250:
	        sm_addrs = 0;
	        result = RP_OK;
		break;

	    case 1000 + 500: 
	    case 1000 + 501: 
	    case 1000 + 552: 
	    case 2000 + 500: 
	    case 2000 + 501:
		result = RP_PARM;
		smtalk (SM_RSET, "RSET");
		free (cp);
		goto losing0;

	    case 2000 + 250:
	    case 2000 + 251:
		sm_addrs++;
	        result = RP_OK;
		break;

	    case 2000 + 451: 
#ifdef SENDMAILBUG
		sm_addrs++;
		result = RP_OK;
		break;
#endif
	    case 2000 + 421: 
	    case 2000 + 450: 
	    case 2000 + 452: 
		result = RP_NO;
		goto bad_addr;

	    case 2000 + 550: 
	    case 2000 + 551: 
	    case 2000 + 552: 
	    case 2000 + 553: 
		result = RP_USER;
bad_addr:
		if (k <= 0 || strcmp (sender, "<>\r\n") == 0)
		    break;
		if (gp == NULL) {
		    int	    l;
		    snprintf (buffer, sizeof(buffer), "%*.*sA.bulk", k, k, file);
		    if ((gp = fopen (buffer, "w+")) == NULL)
			goto bad_data;
		    fprintf (gp, "MAIL FROM:<>\r\nRCPT TO:%sDATA\r\n", sender);
		    l = strlen (sender);
		    fprintf (gp,
			     "To: %*.*s\r\nSubject: Invalid addresses (%s)\r\n",
			     l - 4, l - 4, sender + 1, file);
		    fprintf (gp, "Date: %s\r\nFrom: Postmaster@%s\r\n\r\n",
			     dtimenow (0), LocalName ());
		}
		if (bp = strchr(dp, '\r'))
		    *bp = NULL;
		fprintf (gp, "=>        %s\r\n", dp);
		if (bp)
		    *bp = '\r';
		fprintf (gp, "<= %s\r\n", rp_string (result));
		fflush (gp);
		break;

	    case 3000 + 354: 
#ifdef SENDMAILBUG
ok_data:
#endif
		result = RP_OK;
		break;

	    case 3000 + 451: 
#ifdef SENDMAILBUG
		goto ok_data;
#endif
	    case 3000 + 421:
		result = RP_NO;
bad_data:
		smtalk (SM_RSET, "RSET");
		free (cp);
		if (gp) {
		    snprintf (buffer, sizeof(buffer), "%*.*sA.bulk", k, k, file);
		    unlink (buffer);
		    fclose (gp);
		}
		fclose (fp);
		return result;

	    case 3000 + 500: 
	    case 3000 + 501: 
	    case 3000 + 503: 
	    case 3000 + 554: 
		smtalk (SM_RSET, "RSET");
		free (cp);
		goto no_dice;

	    default:
		result = RP_RPLY;
		goto bad_data;
	}
    }
    free (cp);

    {
#ifdef HAVE_ST_BLKSIZE
	struct stat st;

	if (fstat (fileno (sm_wfp), &st) == NOTOK || (cc = st.st_blksize) < BUFSIZ)
	    cc = BUFSIZ;
#else
	cc = BUFSIZ;
#endif
	if ((cp = malloc ((size_t) cc)) == NULL) {
	    smtalk (SM_RSET, "RSET");
	    sm_reply.length = strlen (strcpy (sm_reply.text, "out of memory"));
	    goto losing1;
	}
    }

    fseek (fp, pos, SEEK_SET);
    for (;;) {
	int eof = 0;

	for (dp = cp, i = cc; i > 0; dp += j, i -= j)
	    if ((j = fread (cp, sizeof(*cp), i, fp)) == OK) {
		if (ferror (fp)) {
		    int len;

		    snprintf (sm_reply.text, sizeof(sm_reply.text),
			"error reading %s: ", file);
		    bp = sm_reply.text;
		    len = strlen (bp);
		    bp += len;
		    if ((s = strerror (errno)))
			strncpy (bp, s, sizeof(sm_reply.text) - len);
		    else
			snprintf (bp, sizeof(sm_reply.text) - len,
				"unknown error %d", errno);
		    sm_reply.length = strlen (sm_reply.text);
		    goto losing2;
		}
		cc = dp - cp;
		eof = 1;
		break;
	    }

	for (dp = cp, i = cc; i > 0; dp += j, i -= j)
	    if ((j = write (fileno (sm_wfp), dp, i)) == NOTOK)
		goto losing3;
	    else
		if (sm_debug) {
		    printf ("wrote %d octets to server\n", j);
		    fflush (stdout);
		}

	if (eof)
	    break;
    }
    free (cp);

    switch (smhear ()) {
	case 250: 
	case 251: 
#ifdef SENDMAILBUG
ok_dot:
#endif
	    result = RP_OK;
	    unlink (file);
	    break;

	case 451: 
#ifdef SENDMAILBUG
	    goto ok_dot;
#endif
	case 452: 
	default: 
	    result = RP_NO;
	    if (gp) {
		snprintf (buffer, sizeof(buffer), "%*.*sA.bulk", k, k, file);
		unlink (buffer);
		fclose (gp);
		gp = NULL;
	    }
	    break;

	case 552: 
	case 554: 
no_dice:
	    result = RP_NDEL;
	    if (k <= 0 || strcmp (sender, "<>\r\n") == 0) {
		unlink (file);
		break;
	    }
	    if (gp) {
		fflush (gp);
		ftruncate (fileno (gp), 0L);
		fseek (gp, 0L, SEEK_SET);
	    }
    	    else {
		snprintf (buffer, sizeof(buffer), "%*.*sA.bulk", k, k, file);
		if ((gp = fopen (buffer, "w")) == NULL)
		    break;
	    }
	    fprintf (gp, "MAIL FROM:<>\r\nRCPT TO:%sDATA\r\n", sender);
	    i = strlen (sender);
	    fprintf (gp, "To: %*.*s\r\nSubject: Failed mail (%s)\r\n",
		     i - 4, i - 4, sender + 1, file);
            fprintf (gp, "Date: %s\r\nFrom: Postmaster@%s\r\n\r\n",
		     dtimenow (0), LocalName ());
	    break;
    }

    if (gp) {
	fputs ("\r\n------- Begin Returned message\r\n\r\n", gp);
	fseek (fp, pos, SEEK_SET);
	while (fgets (buffer, sizeof(buffer), fp)) {
	    if (buffer[0] == '-')
		fputs ("- ", gp);
	    if (strcmp (buffer, ".\r\n"))
		fputs (buffer, gp);
	}
	fputs ("\r\n------- End Returned Message\r\n\r\n.\r\n", gp);
	fflush (gp);
	if (!ferror (gp))
	    unlink (file);
	fclose (gp);
    }
    fclose (fp);

    return result;
}
#endif /* MPOP */


#ifdef CYRUS_SASL
/*
 * This function implements SASL authentication for SMTP.  If this function
 * completes successfully, then authentication is successful and we've
 * (optionally) negotiated a security layer.
 *
 * Right now we don't support session encryption.
 */
static int
sm_auth_sasl(char *user, char *mechlist, char *host)
{
    int result, status, outlen;
    unsigned int buflen;
    char *buf, outbuf[BUFSIZ];
    const char *chosen_mech;
    sasl_security_properties_t secprops;
    sasl_external_properties_t extprops;
    sasl_ssf_t *ssf;
    int *outbufmax;

    /*
     * Initialize the callback contexts
     */

    if (user == NULL)
	user = getusername();

    callbacks[SM_SASL_N_CB_USER].context = user;

    /*
     * This is a _bit_ of a hack ... but if the hostname wasn't supplied
     * to us on the command line, then call getpeername and do a
     * reverse-address lookup on the IP address to get the name.
     */

    if (!host) {
	struct sockaddr_in sin;
	int len = sizeof(sin);
	struct hostent *hp;

	if (getpeername(fileno(sm_wfp), (struct sockaddr *) &sin, &len) < 0) {
	    sm_ierror("getpeername on SMTP socket failed: %s",
		      strerror(errno));
	    return NOTOK;
	}

	if ((hp = gethostbyaddr((void *) &sin.sin_addr, sizeof(sin.sin_addr),
				sin.sin_family)) == NULL) {
	    sm_ierror("DNS lookup on IP address %s failed",
		      inet_ntoa(sin.sin_addr));
	    return NOTOK;
	}

	host = strdup(hp->h_name);
    }

    sasl_pw_context[0] = host;
    sasl_pw_context[1] = user;

    callbacks[SM_SASL_N_CB_PASS].context = sasl_pw_context;

    result = sasl_client_init(callbacks);

    if (result != SASL_OK) {
	sm_ierror("SASL library initialization failed: %s",
		  sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    result = sasl_client_new("smtp", host, NULL, SASL_SECURITY_LAYER, &conn);

    if (result != SASL_OK) {
	sm_ierror("SASL client initialization failed: %s",
		  sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    /*
     * Initialize the security properties
     */

    memset(&secprops, 0, sizeof(secprops));
    secprops.maxbufsize = BUFSIZ;
    secprops.max_ssf = 0;	/* XXX change this when we do encryption */
    memset(&extprops, 0, sizeof(extprops));

    result = sasl_setprop(conn, SASL_SEC_PROPS, &secprops);

    if (result != SASL_OK) {
	sm_ierror("SASL security property initialization failed: %s",
		  sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    result = sasl_setprop(conn, SASL_SSF_EXTERNAL, &extprops);

    if (result != SASL_OK) {
	sm_ierror("SASL external property initialization failed: %s",
		  sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    /*
     * Start the actual protocol.  Feed the mech list into the library
     * and get out a possible initial challenge
     */

    result = sasl_client_start(conn, mechlist, NULL, NULL, &buf, &buflen,
			       &chosen_mech);

    if (result != SASL_OK && result != SASL_CONTINUE) {
	sm_ierror("SASL client start failed: %s",
		  sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    /*
     * If we got an initial challenge, send it as part of the AUTH
     * command; otherwise, just send a plain AUTH command.
     */

    if (buflen) {
	status = sasl_encode64(buf, buflen, outbuf, sizeof(outbuf), NULL);
	free(buf);
	if (status != SASL_OK) {
	    sm_ierror("SASL base64 encode failed: %s",
		      sasl_errstring(status, NULL, NULL));
	    return NOTOK;
	}

	status = smtalk(SM_AUTH, "AUTH %s %s", chosen_mech, outbuf);
    } else
	status = smtalk(SM_AUTH, "AUTH %s", chosen_mech);

    /*
     * Now we loop until we either fail, get a SASL_OK, or a 235
     * response code.  Receive the challenges and process them until
     * we're all done.
     */

    while (result == SASL_CONTINUE) {

	/*
	 * If we get a 235 response, that means authentication has
	 * succeeded and we need to break out of the loop (yes, even if
	 * we still get SASL_CONTINUE from sasl_client_step()).
	 *
	 * Otherwise, if we get a message that doesn't seem to be a
	 * valid response, then abort
	 */

	if (status == 235)
	    break;
	else if (status < 300 || status > 399)
	    return RP_BHST;
	
	/*
	 * Special case; a zero-length response from the SMTP server
	 * is returned as a single =.  If we get that, then set buflen
	 * to be zero.  Otherwise, just decode the response.
	 */
	
	if (strcmp("=", sm_reply.text) == 0) {
	    outlen = 0;
	} else {
	    result = sasl_decode64(sm_reply.text, sm_reply.length,
				   outbuf, &outlen);
	
	    if (result != SASL_OK) {
		smtalk(SM_AUTH, "*");
		sm_ierror("SASL base64 decode failed: %s",
			  sasl_errstring(result, NULL, NULL));
		return NOTOK;
	    }
	}

	result = sasl_client_step(conn, outbuf, outlen, NULL, &buf, &buflen);

	if (result != SASL_OK && result != SASL_CONTINUE) {
	    smtalk(SM_AUTH, "*");
	    sm_ierror("SASL client negotiation failed: %s",
		      sasl_errstring(result, NULL, NULL));
	    return NOTOK;
	}

	status = sasl_encode64(buf, buflen, outbuf, sizeof(outbuf), NULL);
	free(buf);

	if (status != SASL_OK) {
	    smtalk(SM_AUTH, "*");
	    sm_ierror("SASL base64 encode failed: %s",
		      sasl_errstring(status, NULL, NULL));
	    return NOTOK;
	}
	
	status = smtalk(SM_AUTH, outbuf);
    }

    /*
     * Make sure that we got the correct response
     */

    if (status < 200 || status > 299)
	return RP_BHST;

    /*
     * Depending on the mechanism, we need to do a FINAL call to
     * sasl_client_step().  Do that now.
     */

    result = sasl_client_step(conn, NULL, 0, NULL, &buf, &buflen);

    if (result != SASL_OK) {
	sm_ierror("SASL final client negotiation failed: %s",
		  sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    /*
     * We _should_ have completed the authentication successfully.
     * Get a few properties from the authentication exchange.
     */

    result = sasl_getprop(conn, SASL_MAXOUTBUF, (void **) &outbufmax);

    if (result != SASL_OK) {
	sm_ierror("Cannot retrieve SASL negotiated output buffer size: %s",
		  sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    maxoutbuf = *outbufmax;

    result = sasl_getprop(conn, SASL_SSF, (void **) &ssf);

    sasl_ssf = *ssf;

    if (result != SASL_OK) {
	sm_ierror("Cannot retrieve SASL negotiated security strength "
		  "factor: %s", sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    if (maxoutbuf == 0 || maxoutbuf > BUFSIZ)
	maxoutbuf = BUFSIZ;

    sasl_complete = 1;

    return RP_OK;
}

/*
 * Our callback functions to feed data to the SASL library
 */

static int
sm_get_user(void *context, int id, const char **result, unsigned *len)
{
    char *user = (char *) context;

    if (! result || id != SASL_CB_USER)
	return SASL_BADPARAM;

    *result = user;
    if (len)
	*len = strlen(user);

    return SASL_OK;
}

static int
sm_get_pass(sasl_conn_t *conn, void *context, int id,
	    sasl_secret_t **psecret)
{
    char **pw_context = (char **) context;
    char *pass = NULL;
    int len;

    if (! psecret || id != SASL_CB_PASS)
	return SASL_BADPARAM;

    ruserpass(pw_context[0], &(pw_context[1]), &pass);

    len = strlen(pass);

    *psecret = (sasl_secret_t *) malloc(sizeof(sasl_secret_t) + len);

    if (! *psecret) {
	free(pass);
	return SASL_NOMEM;
    }

    (*psecret)->len = len;
    strcpy((*psecret)->data, pass);
/*    free(pass); */

    return SASL_OK;
}
#endif /* CYRUS_SASL */

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
    va_list ap;
    int result;
    char buffer[BUFSIZ];

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


/*
 * write the buffer to the open SMTP channel
 */

static int
sm_wrecord (char *buffer, int len)
{
    if (sm_wfp == NULL)
	return sm_werror ();

    fwrite (buffer, sizeof(*buffer), len, sm_wfp);
    fputs ("\r\n", sm_wfp);
    fflush (sm_wfp);

    return (ferror (sm_wfp) ? sm_werror () : OK);
}


static int
sm_wstream (char *buffer, int len)
{
    char  *bp;
    static char lc = '\0';

    if (sm_wfp == NULL)
	return sm_werror ();

    if (buffer == NULL && len == 0) {
	if (lc != '\n')
	    fputs ("\r\n", sm_wfp);
	lc = '\0';
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


/*
 * On some systems, strlen and strcpy are defined as preprocessor macros.  This
 * causes compile problems with the #ifdef MPOP in the middle.  Should the
 * #ifdef MPOP be removed, remove these #undefs.
 */
#ifdef strlen
# undef strlen
#endif
#ifdef strcpy
# undef strcpy
#endif

static int
sm_werror (void)
{
    sm_reply.length =
	strlen (strcpy (sm_reply.text, sm_wfp == NULL ? "no socket opened"
	    : sm_alarmed ? "write to socket timed out"
#ifdef MPOP
	    : sm_ispool ? "error writing to spool file"
#endif
	    : "error writing to socket"));

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
	    char *ep;

	    for (ehlo = EHLOkeys; *ehlo; ehlo++) {
		ep = *ehlo;
		free (ep);
	    }
	} else {
	    at_least_once = 1;
	}

	ehlo = EHLOkeys;
	*ehlo = NULL;
    }

again: ;

    sm_reply.length = 0;
    sm_reply.text[0] = 0;
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
    if (sm_mts == MTS_SMTP)
	sm_reply.length =
	    strlen (strcpy (sm_reply.text, sm_rfp == NULL ? "no socket opened"
		: sm_alarmed ? "read from socket timed out"
		: feof (sm_rfp) ? "premature end-of-file on socket"
		: "error reading from socket"));
    else
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
    char *text;
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


#ifdef MPOP

static char *broken[MAXARGS + 1];

static char **
smail_brkstring (char *strg, char *brksep, char *brkterm)
{
    int bi;
    char c, *sp;

    sp = strg;

    for (bi = 0; bi < MAXARGS; bi++) {
	while (smail_brkany (c = *sp, brksep))
	    *sp++ = 0;
	if (!c || smail_brkany (c, brkterm)) {
	    *sp = 0;
	    broken[bi] = 0;
	    return broken;
	}

	broken[bi] = sp;
	while ((c = *++sp) && !smail_brkany (c, brksep) && !smail_brkany (c, brkterm))
	    continue;
    }
    broken[MAXARGS] = 0;

    return broken;
}


/*
 * returns 1 if chr in strg, 0 otherwise
 */
static int
smail_brkany (char chr, char *strg)
{
    char *sp;
 
    if (strg)
	for (sp = strg; *sp; sp++)
	    if (chr == *sp)
		return 1;
    return 0;
}

/*
 * copy a string array and return pointer to end
 */
char **
smail_copyip (char **p, char **q, int len_q)
{
    while (*p && --len_q > 0)
	*q++ = *p++;

    *q = NULL;
 
    return q;
}

#endif /* MPOP */


static char *
EHLOset (char *s)
{
    size_t len;
    char *ep, **ehlo;

    len = strlen (s);

    for (ehlo = EHLOkeys; *ehlo; ehlo++) {
	ep = *ehlo;
	if (strncmp (ep, s, len) == 0) {
	    for (ep += len; *ep == ' '; ep++)
		continue;
	    return ep;
	}
    }

    return 0;
}
