/* smtp.c -- nmh SMTP interface
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
#include "sbr/base64.h"

/*
 * This module implements an interface to SendMail very similar
 * to the MMDF mm_(3) routines.  The sm_() routines herein talk
 * SMTP to a sendmail process, mapping SMTP reply codes into
 * RP_-style codes.
 */

#define	NBITS ((sizeof (int)) * 8)

/* Timeout in seconds for SMTP commands.
 * Lore has it they must be distinct. */
#define	SM_OPEN	 300      /* Changed to 5 minutes to comply with a SHOULD in RFC 1123 */
#define	SM_HELO	 20
#define	SM_RSET	 15
#define	SM_MAIL	 301      /* changed to 5 minutes and a second (for uniqueness), see above */
#define	SM_RCPT	 302      /* see above */
#define	SM_DATA	 120      /* see above */
#define	SM_DOT	600	/* see above */
#define	SM_QUIT	 30

static int sm_addrs = 0;
static int sm_child = NOTOK;
static int sm_debug = 0;
static bool sm_nl = true;
static int sm_verbose = 0;
static netsec_context *nsc = NULL;

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
static int sendmail_init (char *, int, int, int, int, const char *,
			  const char *);

static int rclient (char *, char *, char **);
static int sm_ierror (const char *fmt, ...) CHECK_PRINTF(1, 2);
static int sm_nerror (char *);
static int smtalk (int time, char *fmt, ...) CHECK_PRINTF(2, 3);
static int sm_wstream (char *, int);
static int smhear (void);
static char *EHLOset (char *) PURE;
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

    return sendmail_init (client, watch, verbose, debug, sasl,
                          saslmech, user);
}

static int
smtp_init (char *client, char *server, char *port, int watch, int verbose,
	   int debug, int sasl, const char *saslmech, const char *user,
           const char *oauth_svc, int tls)
{
    int result, sd1;
    char *errstr, *chosen_server;

    if (watch)
	verbose = true;

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

        /*
         * Last-ditch check just in case client still isn't set to anything
         */
        if (client == NULL || *client == '\0')
            client = "localhost";
    }

    nsc = netsec_init();

    if (user)
	netsec_set_userid(nsc, user);

    if ((sd1 = rclient (server, port, &chosen_server)) == NOTOK)
	return RP_BHST;

    SIGNAL (SIGPIPE, SIG_IGN);

    netsec_set_fd(nsc, sd1, sd1);

    netsec_set_hostname(nsc, chosen_server);

    if (sm_debug)
	netsec_set_snoop(nsc, 1);

    if (sasl) {
	if (netsec_set_sasl_params(nsc, "smtp", saslmech, sm_sasl_callback,
				   &errstr) != OK)
	    return sm_nerror(errstr);
    }

    if (oauth_svc) {
	if (netsec_set_oauth_service(nsc, oauth_svc) != OK)
	    return sm_ierror("OAuth2 not supported");
    }

    if (tls & S_TLSENABLEMASK) {
	if (netsec_set_tls(nsc, 1, tls & S_NOVERIFY, &errstr) != OK)
	    return sm_nerror(errstr);
    }

    /*
     * If tls == S_INITTLS, that means that the user requested
     * "initial" TLS, which happens right after the connection has
     * opened.  Do that negotiation now
     */

    if (tls & S_INITTLS) {
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

    if (tls & S_STARTTLS) {
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
sendmail_init (char *client, int watch, int verbose, int debug, int sasl,
	       const char *saslmech, const char *user)
{
    unsigned int i, result, vecp;
    int pdi[2], pdo[2];
    char *vec[15], *errstr;

    if (watch)
	verbose = true;

    sm_verbose = verbose;
    sm_debug = debug;
    if (nsc)
	return RP_OK;

    if (client == NULL || *client == '\0') {
	if (clientname)
	    client = clientname;
	else
	    client = LocalName(1);	/* no clientname -> LocalName */

        /*
         * Last-ditch check just in case client still isn't set to anything
         */
        if (client == NULL || *client == '\0')
            client = "localhost";
    }

    nsc = netsec_init();

    if (user)
	netsec_set_userid(nsc, user);

    netsec_set_hostname(nsc, client);

    if (sm_debug)
	netsec_set_snoop(nsc, 1);

    if (sasl) {
	if (netsec_set_sasl_params(nsc, "smtp", saslmech, sm_sasl_callback,
				   &errstr) != OK)
	    return sm_nerror(errstr);
    }

    if (pipe (pdi) == NOTOK)
	return sm_ierror ("no pipes");
    if (pipe (pdo) == NOTOK) {
	close (pdi[0]);
	close (pdi[1]);
	return sm_ierror ("no pipes");
    }

    sm_child = fork();
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
	    SIGNAL (SIGPIPE, SIG_IGN);

	    close (pdi[1]);
	    close (pdo[0]);

	    netsec_set_fd(nsc, pdi[0], pdo[1]);
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
rclient (char *server, char *service, char **chosen_server)
{
    int sd;
    char response[BUFSIZ];

    if (server == NULL)
	server = servers;

    *chosen_server = server;

    if ((sd = client (server, service, response, sizeof(response),
		      sm_debug)) != NOTOK)
	return sd;

    sm_ierror ("%s", response);
    return NOTOK;
}

int
sm_winit (char *from, int smtputf8, int eightbit)
{
    const char *mail_parameters = "";

    if (smtputf8) {
        /* Just for information, if an attempt is made to send to an 8-bit
           address without specifying SMTPUTF8, Gmail responds with
           555 5.5.2 Syntax error.
           Gmail doesn't require the 8BITMIME, but RFC 6531 Sec. 1.2 does. */
        if (EHLOset ("8BITMIME")  &&  EHLOset ("SMTPUTF8")) {
            mail_parameters = " BODY=8BITMIME SMTPUTF8";
        } else {
            inform("SMTP server does not support %s, not sending.\n"
                    "Rebuild message with 7-bit headers, WITHOUT -headerencoding utf-8.",
                    EHLOset ("SMTPUTF8") ? "8BITMIME" : "SMTPUTF8");
            sm_end (NOTOK);
            return RP_UCMD;
        }
    } else if (eightbit) {
        /* Comply with RFC 6152, for messages that have any 8-bit characters
           in their body. */
        if (EHLOset ("8BITMIME")) {
            mail_parameters = " BODY=8BITMIME";
        } else {
            inform("SMTP server does not support 8BITMIME, not sending.\n"
                    "Suggest encoding message for 7-bit transport by setting your\n"
                    "locale to C, and/or specifying *b64 in mhbuild directives.");
            sm_end (NOTOK);
            return RP_UCMD;
        }
    }

    switch (smtalk (SM_MAIL, "MAIL FROM:<%s>%s", from, mail_parameters)) {
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
			     FENDNULL(path), mbox, host)) {
	case 250: 
	case 251: 
	    sm_addrs++;
	    return RP_OK;

	case 451: 
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
	    sm_nl = true;
	    return RP_OK;

	case 451: 
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

    result = sm_wstream (buffer, len);

    return result == NOTOK ? RP_BHST : RP_OK;
}


int
sm_wtend (void)
{
    if (sm_wstream(NULL, 0) == NOTOK)
	return RP_BHST;

    switch (smtalk (SM_DOT + 3 * sm_addrs, ".")) {
	case 250: 
	case 251: 
	    return RP_OK;

	case 451: 
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
	    memcpy (sm_note.text, sm_reply.text, sm_reply.length + 1);
	    /* FALLTHRU */
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
	netsec_shutdown(nsc);
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

    return status ? RP_BHST : RP_OK;
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
		sm_nl = true;
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
		}
		/* FALLTHRU */

	    default: 
		sm_nl = false;
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


static int
smhear (void)
{
    int i, code;
    bool cont, more;
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

    for (more = false; (buffer = netsec_readline(nsc, &buflen,
    						 &errstr)) != NULL ; ) {

	if (doingEHLO
	        && has_prefix(buffer, "250")
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

	cont = false;
	code = atoi ((char *) bp);
	bp += 3, buflen -= 3;
	for (; buflen > 0 && isspace (*bp); bp++, buflen--)
	    continue;
	if (buflen > 0 && *bp == '-') {
	    cont = true;
	    bp++, buflen--;
	    for (; buflen > 0 && isspace (*bp); bp++, buflen--)
		continue;
	}

	if (more) {
	    if (code != sm_reply.code || cont)
		continue;
	    more = false;
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
	    if (more && (int) rc > i + 1) {
		memcpy (rp, sm_moreply, i); /* safe because of check in if() */
		rp += i;
		rc -= i;
	    }
	}
	if (more)
	    continue;
	if (sm_reply.code < 100) {
	    if (sm_verbose) {
		puts(sm_reply.text);
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


char *
rp_string (int code)
{
    char *text;
    /* The additional space is to avoid warning from gcc -Wformat-truncation. */
    static char buffer[BUFSIZ + 19];

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
	if (has_prefix(ep, s)) {
	    for (ep += len; *ep == ' '; ep++)
		continue;
	    return ep;
	}
    }

    return 0;
}

/*
 * Our SASL callback; we are either given SASL tokens to generate network
 * protocols messages for, or we decode incoming protocol messages and
 * convert them to binary SASL tokens to pass up into the SASL library.
 */

static int
sm_sasl_callback(enum sasl_message_type mtype, unsigned const char *indata,
		 unsigned int indatalen, unsigned char **outdata,
		 unsigned int *outdatalen, char **errstr)
{
    int rc, snoopoffset;
    char *mech, *line;
    size_t len;

    switch (mtype) {
    case NETSEC_SASL_START:
	/*
	 * Generate an AUTH message; if we were given an input token
	 * then generate a an AUTH message that includes the initial
	 * response.
	 */

	mech = netsec_get_sasl_mechanism(nsc);

	if (indatalen) {
	    char *b64data;

	    b64data = mh_xmalloc(BASE64SIZE(indatalen));
	    writeBase64raw(indata, indatalen, (unsigned char *) b64data);

	    netsec_set_snoop_callback(nsc, netsec_b64_snoop_decoder,
				      &snoopoffset);
	    snoopoffset = 6 + strlen(mech);
	    rc = netsec_printf(nsc, errstr, "AUTH %s %s\r\n", mech, b64data);
	    free(b64data);
	    netsec_set_snoop_callback(nsc, NULL, NULL);
	} else {
	    rc = netsec_printf(nsc, errstr, "AUTH %s\r\n", mech);
	}

	if (rc != OK)
	    return NOTOK;

	if (netsec_flush(nsc, errstr) != OK)
	    return NOTOK;

	break;

    case NETSEC_SASL_READ:
	/*
	 * Read in a line that should contain a 334 response code, followed
	 * by base64 response data.
	 */

	netsec_set_snoop_callback(nsc, netsec_b64_snoop_decoder, &snoopoffset);
	snoopoffset = 4;
	line = netsec_readline(nsc, &len, errstr);
	netsec_set_snoop_callback(nsc, NULL, NULL);

	if (line == NULL)
	    return NOTOK;

	if (len < 4) {
	    netsec_err(errstr, "Invalid format for SASL response");
	    return NOTOK;
	}

	if (!has_prefix(line, "334 ")) {
	    netsec_err(errstr, "Improper SASL protocol response: %s", line);
	    return NOTOK;
	}

	if (len == 4) {
	    *outdata = NULL;
	    *outdatalen = 0;
	} else {
	    rc = decodeBase64(line + 4, outdata, &len, 0, NULL);
	    if (rc != OK) {
		netsec_err(errstr, "Unable to decode base64 response");
		return NOTOK;
	    }
	    *outdatalen = len;
	}
	break;

    case NETSEC_SASL_WRITE:
	/*
	 * The output encoding is pretty simple, so this is easy.
	 */
	if (indatalen == 0) {
	    rc = netsec_printf(nsc, errstr, "\r\n");
	} else {
	    unsigned char *b64data;
	    b64data = mh_xmalloc(BASE64SIZE(indatalen));
	    writeBase64raw(indata, indatalen, b64data);
	    netsec_set_snoop_callback(nsc, netsec_b64_snoop_decoder, NULL);
	    rc = netsec_printf(nsc, errstr, "%s\r\n", b64data);
	    netsec_set_snoop_callback(nsc, NULL, NULL);
	    free(b64data);
	}

	if (rc != OK)
	    return NOTOK;

	if (netsec_flush(nsc, errstr) != OK)
	    return NOTOK;
	break;

    case NETSEC_SASL_FINISH:
	/*
	 * Finish the protocol; we're looking for a 235 message.
	 */
	line = netsec_readline(nsc, &len, errstr);
	if (line == NULL)
	    return NOTOK;

	if (!has_prefix(line, "235 ")) {
	    if (len > 4)
		netsec_err(errstr, "Authentication failed: %s", line + 4);
	    else
		netsec_err(errstr, "Authentication failed: %s", line);
	    return NOTOK;
	}
	break;

    case NETSEC_SASL_CANCEL:
	/*
	 * Cancel the SASL exchange; this is done by sending a single "*".
	 */
	rc = netsec_printf(nsc, errstr, "*\r\n");
	if (rc == OK)
	    rc = netsec_flush(nsc, errstr);
	if (rc != OK)
	    return NOTOK;
	break;
    }

    return OK;
}
