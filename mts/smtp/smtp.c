/*
 * smtp.c -- nmh SMTP interface
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include "smtp.h"
#include <h/mts.h>
#include <h/signals.h>
#include <h/utils.h>
#include <h/netsec.h>

#include <sys/socket.h>

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
#define	SM_OPEN	 300      /* Changed to 5 minutes to comply with a SHOULD in RFC 1123 */
#define	SM_HELO	 20
#define	SM_RSET	 15
#define	SM_MAIL	 301      /* changed to 5 minutes and a second (for uniqueness), see above */
#define	SM_RCPT	 302      /* see above */
#define	SM_DATA	 120      /* see above */
#define	SM_TEXT	180	/* see above */
#define	SM_DOT	600	/* see above */
#define	SM_QUIT	 30
#define	SM_CLOS	 10
#define	SM_AUTH  45

static int sm_addrs = 0;
static int sm_alarmed = 0;
static int sm_child = NOTOK;
static int sm_debug = 0;
static int sm_nl = TRUE;
static int sm_verbose = 0;
static netsec_context *nsc = NULL;

static int next_line_encoded = 0;

#if 0
#ifdef CYRUS_SASL
/*
 * Some globals needed by SASL
 */

static sasl_conn_t *conn = NULL;	/* SASL connection state */
static int sasl_complete = 0;		/* Has authentication succeeded? */
static sasl_ssf_t sasl_ssf;		/* Our security strength factor */
static int maxoutbuf;			/* Maximum crypto output buffer */
static char *sasl_outbuffer;		/* SASL output buffer for encryption */
static int sasl_outbuflen;		/* Current length of data in outbuf */
static int sm_get_user(void *, int, const char **, unsigned *);
static int sm_get_pass(sasl_conn_t *, void *, int, sasl_secret_t **);

static sasl_callback_t callbacks[] = {
    { SASL_CB_USER, (sasl_callback_ft) sm_get_user, NULL },
#define SM_SASL_N_CB_USER 0
    { SASL_CB_AUTHNAME, (sasl_callback_ft) sm_get_user, NULL },
#define SM_SASL_N_CB_AUTHNAME 1
    { SASL_CB_PASS, (sasl_callback_ft) sm_get_pass, NULL },
#define SM_SASL_N_CB_PASS 2
    { SASL_CB_LIST_END, NULL, NULL },
};

#else /* CYRUS_SASL */
int sasl_ssf = 0;
#endif /* CYRUS_SASL */
#endif

#if 0
#ifdef TLS_SUPPORT
static SSL_CTX *sslctx = NULL;
static SSL *ssl = NULL;
static BIO *sbior = NULL;
static BIO *sbiow = NULL;
static BIO *io = NULL;

static int tls_negotiate(void);
#endif /* TLS_SUPPORT */
#endif

#if 0
#if defined(CYRUS_SASL) || defined(TLS_SUPPORT)
#define SASL_MAXRECVBUF 65536
static int sm_fgetc(FILE *);
static char *sasl_inbuffer;		/* SASL input buffer for encryption */
static char *sasl_inptr;		/* Pointer to current inbuf position */
static int sasl_inbuflen;		/* Current length of data in inbuf */
#else
#define sm_fgetc fgetc
#endif

static int tls_active = 0;
#endif

static char *sm_noreply = "No reply text given";
static char *sm_moreply = "; ";
static struct smtp sm_reply;

#define	MAXEHLO	20

static int doingEHLO;
static char *EHLOkeys[MAXEHLO + 1];

/*
 * static prototypes
 */
static int smtp_init (char *, char *, char *, int, int, int, int, const char *,
		      const char *, const char *, int);
static int sendmail_init (char *, char *, int, int, int, int, const char *,
			  const char *);

static int rclient (char *, char *);
static int sm_ierror (const char *fmt, ...);
static int sm_nerror (char *);
static int smtalk (int time, char *fmt, ...);
static int sm_wstream (char *, int);
static int smhear (void);
static int sm_rrecord (char *, int *);
static int sm_rerror (int);
static void alrmser (int);
static char *EHLOset (char *);
static char *prepare_for_display (const char *, int *);
#if 0
static int sm_fwrite(char *, int);
static int sm_fputs(char *);
static int sm_fputc(int);
static void sm_fflush(void);
static int sm_fgets(char *, int, FILE *);
#endif
static int sm_sasl_callback(enum sasl_message_type, unsigned const char *,
			    unsigned int, unsigned char **, unsigned int *,
			    char **);

int
sm_init (char *client, char *server, char *port, int watch, int verbose,
         int debug, int sasl, const char *saslmech, const char *user,
         const char *oauth_svc, int tls)
{
    if (sm_mts == MTS_SMTP)
	return smtp_init (client, server, port, watch, verbose,
			  debug, sasl, saslmech, user, oauth_svc, tls);
    else
	return sendmail_init (client, server, watch, verbose,
                              debug, sasl, saslmech, user);
}

static int
smtp_init (char *client, char *server, char *port, int watch, int verbose,
	   int debug, int sasl, const char *saslmech, const char *user,
           const char *oauth_svc, int tls)
{
    int result, sd1;
    char *errstr;

    if (watch)
	verbose = TRUE;

    sm_verbose = verbose;
    sm_debug = debug;

    if (nsc != NULL)
	goto send_options;

    if (client == NULL || *client == '\0') {
	if (clientname) {
	    client = clientname;
	} else {
	    client = LocalName(1);	/* no clientname -> LocalName */
	}
    }

    /*
     * Last-ditch check just in case client still isn't set to anything
     */

    if (client == NULL || *client == '\0')
	client = "localhost";

    nsc = netsec_init();

    if (user)
	netsec_set_userid(nsc, user);

    if (sm_debug)
	netsec_set_snoop(nsc, 1);

    if (sasl) {
	if (netsec_set_sasl_params(nsc, client, "smtp", saslmech,
				   sm_sasl_callback, &errstr) != OK)
	    return sm_nerror(errstr);
    }

    if (oauth_svc) {
	if (netsec_set_oauth_service(nsc, oauth_svc) != OK)
	    return sm_ierror("OAuth2 not supported");
    }

    if ((sd1 = rclient (server, port)) == NOTOK)
	return RP_BHST;

#if 0
    SIGNAL (SIGALRM, alrmser);
#endif
    SIGNAL (SIGPIPE, SIG_IGN);

    netsec_set_fd(nsc, sd1, sd1);

    if (tls) {
	if (netsec_set_tls(nsc, 1, &errstr) != OK)
	    return sm_nerror(errstr);
    }

    /*
     * If tls == 2, that means that the user requested "initial" TLS,
     * which happens right after the connection has opened.  Do that
     * negotiation now
     */

    if (tls == 2) {
    	if (netsec_negotiate_tls(nsc, &errstr) != OK) {
	    sm_end(NOTOK);
	    return sm_nerror(errstr);
	}
    }

    netsec_set_timeout(nsc, SM_OPEN);
    result = smhear ();

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

    doingEHLO = 1;
    result = smtalk (SM_HELO, "EHLO %s", client);
    doingEHLO = 0;

    if (result >= 500 && result <= 599)
	result = smtalk (SM_HELO, "HELO %s", client);

    if (result != 250) {
	sm_end (NOTOK);
	return RP_RPLY;
    }

    /*
     * If the user requested TLS support, then try to do the STARTTLS command
     * as part of the initial dialog.  Assuming this works, we then need to
     * restart the EHLO dialog after TLS negotiation is complete.
     */

    if (tls == 1) {
	if (! EHLOset("STARTTLS")) {
	    sm_end(NOTOK);
	    return sm_ierror("SMTP server does not support TLS");
	}

	result = smtalk(SM_HELO, "STARTTLS");

	if (result != 220) {
	    sm_end(NOTOK);
	    return RP_RPLY;
	}

	/*
	 * Okay, the other side should be waiting for us to start TLS
	 * negotiation.  Oblige them.
	 */

	if (netsec_negotiate_tls(nsc, &errstr) != OK) {
	    sm_end(NOTOK);
	    return sm_nerror(errstr);
	}

	doingEHLO = 1;
	result = smtalk (SM_HELO, "EHLO %s", client);
	doingEHLO = 0;

	if (result != 250) {
	    sm_end (NOTOK);
	    return RP_RPLY;
	}
    }

    /*
     * If the user asked for SASL, then check to see if the SMTP server
     * supports it.  Otherwise, error out (because the SMTP server
     * might have been spoofed; we don't want to just silently not
     * do authentication
     */

    if (sasl) {
        char *server_mechs;
	if (! (server_mechs = EHLOset("AUTH"))) {
	    sm_end(NOTOK);
	    return sm_ierror("SMTP server does not support SASL");
	}

	if (netsec_negotiate_sasl(nsc, server_mechs, &errstr) != OK) {
	    sm_end(NOTOK);
	    return sm_nerror(errstr);
	}
    }

send_options: ;
    if (watch && EHLOset ("XVRB"))
	smtalk (SM_HELO, "VERB on");

    return RP_OK;
}

int
sendmail_init (char *client, char *server, int watch, int verbose,
               int debug, int sasl, const char *saslmech, const char *user)
{
    unsigned int i, result, vecp;
    int pdi[2], pdo[2];
    char *vec[15], *errstr;

    if (watch)
	verbose = TRUE;

    sm_verbose = verbose;
    sm_debug = debug;
    if (nsc)
	return RP_OK;

    if (client == NULL || *client == '\0') {
	if (clientname)
	    client = clientname;
	else
	    client = LocalName(1);	/* no clientname -> LocalName */
    }

    /*
     * Last-ditch check just in case client still isn't set to anything
     */

    if (client == NULL || *client == '\0')
	client = "localhost";

    nsc = netsec_init();

    if (user)
	netsec_set_userid(nsc, user);

    if (sm_debug)
	netsec_set_snoop(nsc, 1);

    if (sasl) {
	if (netsec_set_sasl_params(nsc, client, "smtp", saslmech,
				   sm_sasl_callback, &errstr) != OK)
	    return sm_nerror(errstr);
    }

#if 0
    if (oauth_svc) {
	if (netsec_set_oauth_service(nsc, oauth_svc) != OK)
	    return sm_ierror("OAuth2 not supported");
    }
#endif

#if 0
#if defined(CYRUS_SASL) || defined(TLS_SUPPORT)
    sasl_inbuffer = malloc(SASL_MAXRECVBUF);
    if (!sasl_inbuffer)
	return sm_ierror("Unable to allocate %d bytes for read buffer",
			 SASL_MAXRECVBUF);
#endif /* CYRUS_SASL || TLS_SUPPORT */
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
	    vec[vecp++] = watch ? "-odi" : "-odb";
	    vec[vecp++] = "-oem";
	    vec[vecp++] = "-om";
	    if (verbose)
		vec[vecp++] = "-ov";
	    vec[vecp++] = NULL;

	    execvp (sendmail, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (sendmail);
	    _exit (-1);		/* NOTREACHED */

	default: 
	    SIGNAL (SIGALRM, alrmser);
	    SIGNAL (SIGPIPE, SIG_IGN);

	    close (pdi[1]);
	    close (pdo[0]);

	    netsec_set_fd(nsc, pdi[i], pdo[1]);
	    netsec_set_timeout(nsc, SM_OPEN);
	    result = smhear ();
	    switch (result) {
		case 220: 
		    break;

		default: 
		    sm_end (NOTOK);
		    return RP_RPLY;
	    }

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

	    /*
	     * If the user asked for SASL, then check to see if the SMTP server
	     * supports it.  Otherwise, error out (because the SMTP server
	     * might have been spoofed; we don't want to just silently not
	     * do authentication
	     */

	    if (sasl) {
        	char *server_mechs;
		if (! (server_mechs = EHLOset("AUTH"))) {
		    sm_end(NOTOK);
		    return sm_ierror("SMTP server does not support SASL");
		}
		if (netsec_negotiate_sasl(nsc, server_mechs, &errstr) != OK) {
		    sm_end(NOTOK);
		    return sm_nerror(errstr);
		}
	    }

	    if (watch)
		smtalk (SM_HELO, "VERB on");

	    return RP_OK;
    }
}

static int
rclient (char *server, char *service)
{
    int sd;
    char response[BUFSIZ];

    if ((sd = client (server, service, response, sizeof(response),
		      sm_debug)) != NOTOK)
	return sd;

    sm_ierror ("%s", response);
    return NOTOK;
}

int
sm_winit (char *from)
{
    switch (smtalk (SM_MAIL, "MAIL FROM:<%s>", from)) {
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

    if (sm_mts == MTS_SENDMAIL_SMTP) {
	switch (sm_child) {
	    case NOTOK: 
	    case OK: 
		return RP_OK;

	    default: 
		break;
	}
    }

    if (nsc == NULL)
	return RP_OK;

    switch (type) {
	case OK: 
	    smtalk (SM_QUIT, "QUIT");
	    break;

	case NOTOK: 
	    sm_note.code = sm_reply.code;
	    sm_note.length = sm_reply.length;
	    memcpy (sm_note.text, sm_reply.text, sm_reply.length + 1);/* fall */
	case DONE: 
	    if (smtalk (SM_RSET, "RSET") == 250 && type == DONE)
		return RP_OK;
	    if (sm_mts == MTS_SMTP)
		smtalk (SM_QUIT, "QUIT");
	    else {
		/* The SIGPIPE block replaces old calls to discard ().
		   We're not sure what the discard () calls were for,
		   maybe to prevent deadlock on old systems.  In any
		   case, blocking SIGPIPE should be harmless.
		   Because the file handles are closed below, leave it
		   blocked. */
		sigset_t set, oset;
		sigemptyset (&set);
		sigaddset (&set, SIGPIPE);
		sigprocmask (SIG_BLOCK, &set, &oset);

		kill (sm_child, SIGKILL);
		sm_child = NOTOK;
	    }
	    if (type == NOTOK) {
		sm_reply.code = sm_note.code;
		sm_reply.length = sm_note.length;
		memcpy (sm_reply.text, sm_note.text, sm_note.length + 1);
	    }
	    break;
    }

    if (nsc != NULL) {
	alarm (SM_CLOS);
	netsec_shutdown(nsc, 1);
	alarm (0);
	nsc = NULL;
    }

    if (sm_mts == MTS_SMTP) {
	status = 0;
    } else if (sm_child != NOTOK) {
	status = pidwait (sm_child, OK);
	sm_child = NOTOK;
    } else {
	status = OK;
    }

    return (status ? RP_BHST : RP_OK);
}

#if 0
/*
 * This function implements SASL authentication for SMTP.  If this function
 * completes successfully, then authentication is successful and we've
 * (optionally) negotiated a security layer.
 */

#define CHECKB64SIZE(insize, outbuf, outsize) \
    { size_t wantout = (((insize + 2) / 3) * 4) + 32; \
      if (wantout > outsize) { \
          outbuf = mh_xrealloc(outbuf, outsize = wantout); \
      } \
    }

static int
sm_auth_sasl(char *user, int saslssf, char *mechlist, char *inhost)
{
    int result, status;
    unsigned int buflen, outlen;
    char *buf, *outbuf = NULL, host[NI_MAXHOST];
    const char *chosen_mech;
    sasl_security_properties_t secprops;
    sasl_ssf_t *ssf;
    int *outbufmax;
    struct nmh_creds creds = { 0, 0, 0 };
    size_t outbufsize = 0;

    /*
     * Initialize the callback contexts
     */

    /*
     * This is a _bit_ of a hack ... but if the hostname wasn't supplied
     * to us on the command line, then call getpeername and do a
     * reverse-address lookup on the IP address to get the name.
     */

    memset(host, 0, sizeof(host));

    if (!inhost) {
	struct sockaddr_storage sin;
	socklen_t len = sizeof(sin);
	int result;

	if (getpeername(fileno(sm_wfp), (struct sockaddr *) &sin, &len) < 0) {
	    sm_ierror("getpeername on SMTP socket failed: %s",
		      strerror(errno));
	    return NOTOK;
	}

	result = getnameinfo((struct sockaddr *) &sin, len, host, sizeof(host),
			     NULL, 0, NI_NAMEREQD);
	if (result != 0) {
	    sm_ierror("Unable to look up name of connected host: %s",
		      gai_strerror(result));
	    return NOTOK;
	}
    } else {
	strncpy(host, inhost, sizeof(host) - 1);
    }

    /* It's OK to copy the addresses here.  The callbacks that use
       them will only be called before this function returns. */
    creds.host = host;
    creds.user = user;
    callbacks[SM_SASL_N_CB_USER].context = &creds;
    callbacks[SM_SASL_N_CB_AUTHNAME].context = &creds;
    callbacks[SM_SASL_N_CB_PASS].context = &creds;

    result = sasl_client_init(callbacks);

    if (result != SASL_OK) {
	sm_ierror("SASL library initialization failed: %s",
		  sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    result = sasl_client_new("smtp", host, NULL, NULL, NULL, 0, &conn);

    if (result != SASL_OK) {
	sm_ierror("SASL client initialization failed: %s",
		  sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    /*
     * Initialize the security properties.  But if TLS is active, then
     * don't negotiate encryption here.
     */

    memset(&secprops, 0, sizeof(secprops));
    secprops.maxbufsize = SASL_MAXRECVBUF;
    secprops.max_ssf =
      tls_active ? 0 : (saslssf != -1 ? (unsigned int) saslssf : UINT_MAX);

    result = sasl_setprop(conn, SASL_SEC_PROPS, &secprops);

    if (result != SASL_OK) {
	sm_ierror("SASL security property initialization failed: %s",
		  sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    /*
     * Start the actual protocol.  Feed the mech list into the library
     * and get out a possible initial challenge
     */

    result = sasl_client_start(conn, mechlist, NULL, (const char **) &buf,
			       &buflen, (const char **) &chosen_mech);

    if (result != SASL_OK && result != SASL_CONTINUE) {
	sm_ierror("SASL client start failed: %s", sasl_errdetail(conn));
	return NOTOK;
    }

    /*
     * If we got an initial challenge, send it as part of the AUTH
     * command; otherwise, just send a plain AUTH command.
     */

    if (buflen) {
	CHECKB64SIZE(buflen, outbuf, outbufsize);
	status = sasl_encode64(buf, buflen, outbuf, outbufsize, NULL);
	if (status != SASL_OK) {
	    sm_ierror("SASL base64 encode failed: %s",
		      sasl_errstring(status, NULL, NULL));
	    if (outbuf)
		free(outbuf);
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
	else if (status < 300 || status > 399) {
	    if (outbuf)
		free(outbuf);
	    return RP_BHST;
	}
	
	/*
	 * Special case; a zero-length response from the SMTP server
	 * is returned as a single =.  If we get that, then set buflen
	 * to be zero.  Otherwise, just decode the response.
	 */
	
	if (strcmp("=", sm_reply.text) == 0) {
	    outlen = 0;
	} else {
	    if (sm_reply.length > (int) outbufsize) {
		outbuf = mh_xrealloc(outbuf, outbufsize = sm_reply.length);
	    }

	    result = sasl_decode64(sm_reply.text, sm_reply.length,
				   outbuf, outbufsize, &outlen);
	    if (result != SASL_OK) {
		smtalk(SM_AUTH, "*");
		sm_ierror("SASL base64 decode failed: %s",
			  sasl_errstring(result, NULL, NULL));
		if (outbuf)
		    free(outbuf);
		return NOTOK;
	    }
	}

	result = sasl_client_step(conn, outbuf, outlen, NULL,
				  (const char **) &buf, &buflen);

	if (result != SASL_OK && result != SASL_CONTINUE) {
	    smtalk(SM_AUTH, "*");
	    sm_ierror("SASL client negotiation failed: %s",
		      sasl_errstring(result, NULL, NULL));
	    if (outbuf)
		free(outbuf);
	    return NOTOK;
	}

	CHECKB64SIZE(buflen, outbuf, outbufsize);
	status = sasl_encode64(buf, buflen, outbuf, outbufsize, NULL);

	if (status != SASL_OK) {
	    smtalk(SM_AUTH, "*");
	    sm_ierror("SASL base64 encode failed: %s",
		      sasl_errstring(status, NULL, NULL));
	    if (outbuf)
		free(outbuf);
	    return NOTOK;
	}
	
	status = smtalk(SM_AUTH, outbuf);
    }

    if (outbuf)
	free(outbuf);

    /*
     * Make sure that we got the correct response
     */

    if (status < 200 || status > 299)
	return RP_BHST;

    /*
     * We _should_ have completed the authentication successfully.
     * Get a few properties from the authentication exchange.
     */

    result = sasl_getprop(conn, SASL_MAXOUTBUF, (const void **) &outbufmax);

    if (result != SASL_OK) {
	sm_ierror("Cannot retrieve SASL negotiated output buffer size: %s",
		  sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    maxoutbuf = *outbufmax;

    result = sasl_getprop(conn, SASL_SSF, (const void **) &ssf);

    sasl_ssf = *ssf;

    if (result != SASL_OK) {
	sm_ierror("Cannot retrieve SASL negotiated security strength "
		  "factor: %s", sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    if (sasl_ssf > 0) {
	sasl_outbuffer = malloc(maxoutbuf);

	if (sasl_outbuffer == NULL) {
		sm_ierror("Unable to allocate %d bytes for SASL output "
			  "buffer", maxoutbuf);
		return NOTOK;
	}
	sasl_outbuflen = 0;
	sasl_inbuflen = 0;
	sasl_inptr = sasl_inbuffer;
    } else {
	sasl_outbuffer = NULL;
        /* Don't NULL out sasl_inbuffer because it could be used in
           sm_fgetc (). */
    }

    sasl_complete = 1;

    return RP_OK;
}

/*
 * Our callback functions to feed data to the SASL library
 */

static int
sm_get_user(void *context, int id, const char **result, unsigned *len)
{
    nmh_creds_t creds = (nmh_creds_t) context;

    if (! result || ((id != SASL_CB_USER) && (id != SASL_CB_AUTHNAME)))
	return SASL_BADPARAM;

    if (creds->user == NULL) {
        /*
         * Pass the 1 third argument to nmh_get_credentials() so
         * that a default user if the -user switch to send(1)/post(8)
         * wasn't used, and so that a default password will be supplied.
         * That's used when those values really don't matter, and only
         * with legacy/.netrc, i.e., with a credentials profile entry.
         */
        if (nmh_get_credentials (creds->host, creds->user, 1, creds) != OK) {
            return SASL_BADPARAM;
        }
    }

    *result = creds->user;
    if (len)
	*len = strlen(creds->user);

    return SASL_OK;
}

static int
sm_get_pass(sasl_conn_t *conn, void *context, int id,
	    sasl_secret_t **psecret)
{
    nmh_creds_t creds = (nmh_creds_t) context;
    int len;

    NMH_UNUSED (conn);

    if (! psecret || id != SASL_CB_PASS)
	return SASL_BADPARAM;

    if (creds->password == NULL) {
        /*
         * Pass the 0 third argument to nmh_get_credentials() so
         * that the default password isn't used.  With legacy/.netrc
         * credentials support, we'll only get here if the -user
         * switch to send(1)/post(8) wasn't used.
         */
        if (nmh_get_credentials (creds->host, creds->user, 0, creds) != OK) {
            return SASL_BADPARAM;
        }
    }

    len = strlen (creds->password);

    if (! (*psecret = (sasl_secret_t *) malloc(sizeof(sasl_secret_t) + len))) {
	return SASL_NOMEM;
    }

    (*psecret)->len = len;
    strcpy((char *) (*psecret)->data, creds->password);

    return SASL_OK;
}
#endif /* CYRUS_SASL */

/* https://developers.google.com/gmail/xoauth2_protocol */
static int
sm_auth_xoauth2(const char *user, const char *oauth_svc, int snoop)
{
    const char *xoauth_client_res;
    int status;

#if 0
    xoauth_client_res = mh_oauth_do_xoauth(user, oauth_svc,
					   snoop ? stderr : NULL);

    if (xoauth_client_res == NULL) {
	return sm_ierror("Internal error: mh_oauth_do_xoauth() returned NULL");
    }
#else
    NMH_UNUSED(user);
    NMH_UNUSED(snoop);
    adios(NULL, "sendfrom built without OAUTH_SUPPORT, "
          "so oauth_svc %s is not supported", oauth_svc);
#endif /* OAUTH_SUPPORT */

    status = smtalk(SM_AUTH, "AUTH XOAUTH2 %s", xoauth_client_res);
    if (status == 235) {
        /* It worked! */
        return RP_OK;
    }

    /*
     * Status is 334 and sm_reply.text contains base64-encoded JSON.  As far as
     * epg can tell, no matter the error, the JSON is always the same:
     * {"status":"400","schemes":"Bearer","scope":"https://mail.google.com/"}
     * I tried these errors:
     * - garbage token
     * - expired token
     * - wrong scope
     * - wrong username
     */
    /* Then we're supposed to send an empty response ("\r\n"). */
    smtalk(SM_AUTH, "");
    /*
     * And now we always get this, again, no matter the error:
     * 535-5.7.8 Username and Password not accepted. Learn more at
     * 535 5.7.8 http://support.google.com/mail/bin/answer.py?answer=14257
     */
    return RP_BHST;
}

static int
sm_ierror (const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf (sm_reply.text, sizeof(sm_reply.text), fmt, ap);
    va_end(ap);

    sm_reply.length = strlen (sm_reply.text);
    sm_reply.code = NOTOK;

    return RP_BHST;
}

/*
 * Like sm_ierror, but assume it's an allocated error string we need to free.
 */

static int
sm_nerror (char *str)
{
    strncpy(sm_reply.text, str, sizeof(sm_reply.text));
    sm_reply.text[sizeof(sm_reply.text) - 1] = '\0';
    sm_reply.length = strlen(sm_reply.text);
    sm_reply.code = NOTOK;
    free(str);

    return RP_BHST;
}

static int
smtalk (int time, char *fmt, ...)
{
    va_list ap;
    char *errstr;
    int result;

    va_start(ap, fmt);
    result = netsec_vprintf (nsc, &errstr, fmt, ap);
    va_end(ap);

    if (result != OK)
	return sm_nerror(errstr);

    if (netsec_printf (nsc, &errstr, "\r\n") != OK)
	return sm_nerror(errstr);

    if (netsec_flush (nsc, &errstr) != OK)
	return sm_nerror(errstr);

    netsec_set_timeout(nsc, time);

    return smhear ();
}


static int
sm_wstream (char *buffer, int len)
{
    char *bp, *errstr;
    static char lc = '\0';
    int rc;

    if (nsc == NULL) {
	sm_ierror("No socket opened");
	return NOTOK;
    }

    if (buffer == NULL && len == 0) {
	rc = OK;
	if (lc != '\n')
	    rc = netsec_write(nsc, "\r\n", 2, &errstr);
	lc = '\0';
	if (rc != OK)
	    sm_nerror(errstr);
	return rc;
    }

    for (bp = buffer; bp && len > 0; bp++, len--) {
	switch (*bp) {
	    case '\n': 
		sm_nl = TRUE;
		if (netsec_write(nsc, "\r", 1, &errstr) != OK) {
		    sm_nerror(errstr);
		    return NOTOK;
		}
		break;

	    case '.': 
		if (sm_nl)
		    if (netsec_write(nsc, ".", 1, &errstr) != OK) {
		    sm_nerror(errstr);
		    return NOTOK;
		} /* FALL THROUGH */

	    default: 
		sm_nl = FALSE;
	}
	if (netsec_write(nsc, bp, 1, &errstr) != OK) {
	    sm_nerror(errstr);
	    return NOTOK;
	}
    }

    if (bp > buffer)
	lc = *--bp;
    return OK;
}

/*
 * Write out to the network, but do buffering for SASL (if enabled)
 */

#if 0
static int
sm_fwrite(char *buffer, int len)
{
#ifdef CYRUS_SASL
    const char *output;
    unsigned int outputlen;

    if (sasl_complete == 0 || sasl_ssf == 0) {
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
	if (tls_active) {
	    int ret;

	    ret = BIO_write(io, buffer, len);

	    if (ret <= 0) {
		sm_ierror("TLS error during write: %s",
			  ERR_error_string(ERR_get_error(), NULL));
		return NOTOK;
	    }
	} else
#endif /* TLS_SUPPORT */
	if ((int) fwrite(buffer, sizeof(*buffer), len, sm_wfp) < len) {
	    advise ("sm_fwrite", "fwrite");
	}
#ifdef CYRUS_SASL
    } else {
	while (len >= maxoutbuf - sasl_outbuflen) {
	    memcpy(sasl_outbuffer + sasl_outbuflen, buffer,
		   maxoutbuf - sasl_outbuflen);
	    len -= maxoutbuf - sasl_outbuflen;
	    sasl_outbuflen = 0;

	    if (sasl_encode(conn, sasl_outbuffer, maxoutbuf,
			    &output, &outputlen) != SASL_OK) {
		sm_ierror("Unable to SASL encode connection data: %s",
			  sasl_errdetail(conn));
		return NOTOK;
	    }

	    if (fwrite(output, sizeof(*output), outputlen, sm_wfp) <
		outputlen) {
		advise ("sm_fwrite", "fwrite");
	    }
	}

	if (len > 0) {
	    memcpy(sasl_outbuffer + sasl_outbuflen, buffer, len);
	    sasl_outbuflen += len;
	}
    }
#endif /* CYRUS_SASL */
    return ferror(sm_wfp) ? NOTOK : RP_OK;
}

static int
sm_werror (void)
{
    sm_reply.length =
	strlen (strcpy (sm_reply.text, sm_wfp == NULL ? "no socket opened"
	    : sm_alarmed ? "write to socket timed out"
	    : "error writing to socket"));

    return (sm_reply.code = NOTOK);
}
#endif


static int
smhear (void)
{
    int i, code, cont, more;
    size_t buflen, rc;
    unsigned char *bp;
    char *rp;
    char *errstr;
    char **ehlo = EHLOkeys, *buffer;

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

    for (more = FALSE; (buffer = netsec_readline(nsc, &buflen,
    						 &errstr)) != NULL ; ) {

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

	bp = (unsigned char *) buffer;

	for (; buflen > 0 && (!isascii (*bp) || !isdigit (*bp)); bp++, buflen--)
	    continue;

	cont = FALSE;
	code = atoi ((char *) bp);
	bp += 3, buflen -= 3;
	for (; buflen > 0 && isspace (*bp); bp++, buflen--)
	    continue;
	if (buflen > 0 && *bp == '-') {
	    cont = TRUE;
	    bp++, buflen--;
	    for (; buflen > 0 && isspace (*bp); bp++, buflen--)
		continue;
	}

	if (more) {
	    if (code != sm_reply.code || cont)
		continue;
	    more = FALSE;
	} else {
	    sm_reply.code = code;
	    more = cont;
	    if (buflen <= 0) {
		bp = (unsigned char *) sm_noreply;
		buflen = strlen (sm_noreply);
	    }
	}

	if ((i = min (buflen, rc)) > 0) {
	    memcpy (rp, bp, i);
	    rp += i;
	    rc -= i;
	    i = strlen(sm_moreply);
	    if (more && rc > i + 1) {
		memcpy (rp, sm_moreply, i); /* safe because of check in if() */
		rp += i;
		rc -= i;
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
	sm_reply.text[sm_reply.length] = 0;
	return sm_reply.code;
    }

    sm_nerror(errstr);
    return NOTOK;
}


#if 0
static int
sm_rrecord (char *buffer, int *len)
{
    int retval;
    size_t strlen;
    char *str, *errstr;

    if (nsc == NULL)
	return 

    buffer[*len = 0] = 0;

    str = netsec_readline(nsc, &strlen, &errstr);
    if ((retval = netsec_readline(nsc, 
    if ((retval = sm_fgets (buffer, BUFSIZ, sm_rfp)) != RP_OK)
	return sm_rerror (retval);
    *len = strlen (buffer);
    /* *len should be >0 except on EOF, but check for safety's sake */
    if (*len == 0)
	return sm_rerror (RP_EOF);
    if (buffer[*len - 1] != '\n')
	while ((retval = sm_fgetc (sm_rfp)) != '\n' && retval != EOF &&
	       retval != -2)
	    continue;
    else
	if ((*len > 1) && (buffer[*len - 2] == '\r'))
	    *len -= 1;
    *len -= 1;
    buffer[*len] = 0;

    return OK;
}

/*
 * Our version of fgets, which calls our private fgetc function
 */

static int
sm_fgets(char *buffer, int size, FILE *f)
{
    int c;

     do {
	c = sm_fgetc(f);

	if (c == EOF)
	    return RP_EOF;

	if (c == -2)
	    return NOTOK;

	*buffer++ = c;
     } while (size > 1 && c != '\n');

     *buffer = '\0';

     return RP_OK;
}


#if defined(CYRUS_SASL) || defined(TLS_SUPPORT)
/*
 * Read from the network, but do SASL or TLS encryption
 */

static int
sm_fgetc(FILE *f)
{
    char tmpbuf[BUFSIZ], *retbuf;
    unsigned int retbufsize = 0;
    int cc, result;

    /*
     * If we have leftover data, return it
     */

    if (sasl_inbuflen) {
	sasl_inbuflen--;
	return (int) *sasl_inptr++;
    }

    /*
     * If not, read from the network until we have some data to return
     */

    while (retbufsize == 0) {

#ifdef TLS_SUPPORT
	if (tls_active) {
	    cc = SSL_read(ssl, tmpbuf, sizeof(tmpbuf));

	    if (cc == 0) {
		result = SSL_get_error(ssl, cc);

		if (result != SSL_ERROR_ZERO_RETURN) {
		    sm_ierror("TLS peer aborted connection");
		}

		return EOF;
	    }

	    if (cc < 0) {
		sm_ierror("SSL_read failed: %s",
			  ERR_error_string(ERR_get_error(), NULL));
		return -2;
	    }
	} else
#endif /* TLS_SUPPORT */

	cc = read(fileno(f), tmpbuf, sizeof(tmpbuf));

	if (cc == 0)
	    return EOF;

	if (cc < 0) {
	    sm_ierror("Unable to read from network: %s", strerror(errno));
	    return -2;
	}

	/*
	 * Don't call sasl_decode unless sasl is complete and we have
	 * encryption working
	 */

#ifdef CYRUS_SASL
	if (sasl_complete == 0 || sasl_ssf == 0) {
	    retbuf = tmpbuf;
	    retbufsize = cc;
	} else {
	    result = sasl_decode(conn, tmpbuf, cc, (const char **) &retbuf,
				 &retbufsize);

	    if (result != SASL_OK) {
		sm_ierror("Unable to decode SASL network data: %s",
			  sasl_errdetail(conn));
		return -2;
	    }
	}
#else /* ! CYRUS_SASL */
	retbuf = tmpbuf;
	retbufsize = cc;
#endif /* CYRUS_SASL */
    }

    if (retbufsize > SASL_MAXRECVBUF) {
	sm_ierror("Received data (%d bytes) is larger than the buffer "
		  "size (%d bytes)", retbufsize, SASL_MAXRECVBUF);
	return -2;
    }

    memcpy(sasl_inbuffer, retbuf, retbufsize);
    sasl_inptr = sasl_inbuffer + 1;
    sasl_inbuflen = retbufsize - 1;

    return (int) sasl_inbuffer[0];
}
#endif /* CYRUS_SASL || TLS_SUPPORT */

static int
sm_rerror (int rc)
{
    if (sm_mts == MTS_SMTP)
	sm_reply.length =
	    strlen (strcpy (sm_reply.text, sm_rfp == NULL ? "no socket opened"
		: sm_alarmed ? "read from socket timed out"
		: rc == RP_EOF ? "premature end-of-file on socket"
		: "error reading from socket"));
    else
	sm_reply.length =
	    strlen (strcpy (sm_reply.text, sm_rfp == NULL ? "no pipe opened"
		: sm_alarmed ? "read from pipe timed out"
		: rc == RP_EOF ? "premature end-of-file on pipe"
		: "error reading from pipe"));

    return (sm_reply.code = NOTOK);
}
#endif


static void
alrmser (int i)
{
    NMH_UNUSED (i);

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


/*
 * Detects, using heuristics, if an SMTP server or client response string
 * contains a base64-encoded portion.  If it does, decodes it and replaces
 * any non-printable characters with a hex representation.  Caller is
 * responsible for free'ing return value.  If the decode fails, a copy of
 * the input string is returned.
 */
static
char *
prepare_for_display (const char *string, int *next_line_encoded) {
    const char *start = NULL;
    const char *decoded;
    size_t decoded_len;
    int prefix_len = -1;

    if (strncmp (string, "AUTH ", 5) == 0) {
        /* AUTH line:  the mechanism isn't encoded.  If there's an initial
           response, it must be base64 encoded.. */
        char *mechanism = strchr (string + 5, ' ');

        if (mechanism != NULL) {
            prefix_len = (int) (mechanism - string + 1);
        } /* else no space following the mechanism, so no initial response */
        *next_line_encoded = 0;
    } else if (strncmp (string, "334 ", 4) == 0) {
        /* 334 is the server's request for user or password. */
        prefix_len = 4;
        /* The next (client response) line must be base64 encoded. */
        *next_line_encoded = 1;
    } else if (*next_line_encoded) {
        /* "next" line now refers to this line, which is a base64-encoded
           client response. */
        prefix_len = 0;
        *next_line_encoded = 0;
    } else {
        *next_line_encoded = 0;
    }

    /* Don't attempt to decoded unencoded initial response ('=') or cancel
       response ('*'). */
    if (prefix_len > -1  &&
        string[prefix_len] != '='  &&  string[prefix_len] != '*') {
        start = string + prefix_len;
    }

    if (start  &&  decodeBase64 (start, &decoded, &decoded_len, 1, NULL) == OK) {
        char *hexified;
        char *prefix = mh_xmalloc(prefix_len + 1);
        char *display_string;

        /* prefix is the beginning portion, which isn't base64 encoded. */
        snprintf (prefix, prefix_len + 1, "%*s", prefix_len, string);
        hexify ((const unsigned char *) decoded, decoded_len, &hexified);
        /* Wrap the decoded portion in "b64<>". */
        display_string = concat (prefix, "b64<", hexified, ">", NULL);
        free (hexified);
        free (prefix);
        free ((char *) decoded);

        return display_string;
    } else {
        return getcpy (string);
    }
}
