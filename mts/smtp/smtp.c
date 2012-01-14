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
#include <signal.h>
#include <h/signals.h>

#ifdef CYRUS_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#endif /* CYRUS_SASL */

#ifdef TLS_SUPPORT
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif /* TLS_SUPPORT */

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
#ifdef CYRUS_SASL
#define	SM_AUTH  45
#endif /* CYRUS_SASL */

static int sm_addrs = 0;
static int sm_alarmed = 0;
static int sm_child = NOTOK;
static int sm_debug = 0;
static int sm_nl = TRUE;
static int sm_verbose = 0;

static FILE *sm_rfp = NULL;
static FILE *sm_wfp = NULL;

#ifdef CYRUS_SASL
/*
 * Some globals needed by SASL
 */

static sasl_conn_t *conn = NULL;	/* SASL connection state */
static int sasl_complete = 0;		/* Has authentication succeded? */
static sasl_ssf_t sasl_ssf;		/* Our security strength factor */
static char *sasl_pw_context[2];	/* Context to pass into sm_get_pass */
static int maxoutbuf;			/* Maximum crypto output buffer */
static char *sasl_outbuffer;		/* SASL output buffer for encryption */
static int sasl_outbuflen;		/* Current length of data in outbuf */
static int sm_get_user(void *, int, const char **, unsigned *);
static int sm_get_pass(sasl_conn_t *, void *, int, sasl_secret_t **);

static sasl_callback_t callbacks[] = {
    { SASL_CB_USER, sm_get_user, NULL },
#define SM_SASL_N_CB_USER 0
    { SASL_CB_PASS, sm_get_pass, NULL },
#define SM_SASL_N_CB_PASS 1
    { SASL_CB_AUTHNAME, sm_get_user, NULL },
#define SM_SASL_N_CB_AUTHNAME 2
    { SASL_CB_LIST_END, NULL, NULL },
};

#else /* CYRUS_SASL */
int sasl_ssf = 0;
#endif /* CYRUS_SASL */

#ifdef TLS_SUPPORT
static SSL_CTX *sslctx = NULL;
static SSL *ssl = NULL;
static BIO *sbior = NULL;
static BIO *sbiow = NULL;
static BIO *io = NULL;
#endif /* TLS_SUPPORT */

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

static char *sm_noreply = "No reply text given";
static char *sm_moreply = "; ";

struct smtp sm_reply;		/* global... */

#define	MAXEHLO	20

static int doingEHLO;
char *EHLOkeys[MAXEHLO + 1];

/*
 * static prototypes
 */
static int smtp_init (char *, char *, char *, int, int, int, int, int, int,
		      char *, char *, int);
static int sendmail_init (char *, char *, int, int, int, int, int, int,
                          char *, char *);

static int rclient (char *, char *);
static int sm_ierror (char *fmt, ...);
static int smtalk (int time, char *fmt, ...);
static int sm_wrecord (char *, int);
static int sm_wstream (char *, int);
static int sm_werror (void);
static int smhear (void);
static int sm_rrecord (char *, int *);
static int sm_rerror (int);
static void alrmser (int);
static char *EHLOset (char *);
static int sm_fwrite(char *, int);
static int sm_fputs(char *);
static int sm_fputc(int);
static void sm_fflush(void);
static int sm_fgets(char *, int, FILE *);

#ifdef CYRUS_SASL
/*
 * Function prototypes needed for SASL
 */

static int sm_auth_sasl(char *, char *, char *);
#endif /* CYRUS_SASL */

int
sm_init (char *client, char *server, char *port, int watch, int verbose,
         int debug, int onex, int queued, int sasl, char *saslmech,
         char *user, int tls)
{
    if (sm_mts == MTS_SMTP)
	return smtp_init (client, server, port, watch, verbose,
			  debug, onex, queued, sasl, saslmech, user, tls);
    else
	return sendmail_init (client, server, watch, verbose,
                              debug, onex, queued, sasl, saslmech, user);
}

static int
smtp_init (char *client, char *server, char *port, int watch, int verbose,
	   int debug, int onex, int queued,
           int sasl, char *saslmech, char *user, int tls)
{
#ifdef CYRUS_SASL
    char *server_mechs;
#else  /* CYRUS_SASL */
    NMH_UNUSED (sasl);
    NMH_UNUSED (saslmech);
    NMH_UNUSED (user);
#endif /* CYRUS_SASL */
    int result, sd1, sd2;

    if (watch)
	verbose = TRUE;

    sm_verbose = verbose;
    sm_debug = debug;

    if (sm_rfp != NULL && sm_wfp != NULL)
	goto send_options;

    if (client == NULL || *client == '\0') {
	if (clientname) {
	    client = clientname;
	} else {
	    client = LocalName();	/* no clientname -> LocalName */
	}
    }

    /*
     * Last-ditch check just in case client still isn't set to anything
     */

    if (client == NULL || *client == '\0')
	client = "localhost";

#if defined(CYRUS_SASL) || defined(TLS_SUPPORT)
    sasl_inbuffer = malloc(SASL_MAXRECVBUF);
    if (!sasl_inbuffer)
	return sm_ierror("Unable to allocate %d bytes for read buffer",
			 SASL_MAXRECVBUF);
#endif /* CYRUS_SASL || TLS_SUPPORT */

    if ((sd1 = rclient (server, port)) == NOTOK)
	return RP_BHST;

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

    tls_active = 0;

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

    doingEHLO = 1;
    result = smtalk (SM_HELO, "EHLO %s", client);
    doingEHLO = 0;

    if (result >= 500 && result <= 599)
	result = smtalk (SM_HELO, "HELO %s", client);

    if (result != 250) {
	sm_end (NOTOK);
	return RP_RPLY;
    }

#ifdef TLS_SUPPORT
    /*
     * If the user requested TLS support, then try to do the STARTTLS command
     * as part of the initial dialog.  Assuming this works, we then need to
     * restart the EHLO dialog after TLS negotiation is complete.
     */

    if (tls) {
	BIO *ssl_bio;

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

	if (! sslctx) {
	    SSL_METHOD *method;

	    SSL_library_init();
	    SSL_load_error_strings();

	    method = TLSv1_client_method();	/* Not sure about this */

	    sslctx = SSL_CTX_new(method);

	    if (! sslctx) {
		sm_end(NOTOK);
		return sm_ierror("Unable to initialize OpenSSL context: %s",
				 ERR_error_string(ERR_get_error(), NULL));
	    }
	}

	ssl = SSL_new(sslctx);

	if (! ssl) {
	    sm_end(NOTOK);
	    return sm_ierror("Unable to create SSL connection: %s",
			     ERR_error_string(ERR_get_error(), NULL));
	}

	sbior = BIO_new_socket(fileno(sm_rfp), BIO_NOCLOSE);
	sbiow = BIO_new_socket(fileno(sm_wfp), BIO_NOCLOSE);

	if (sbior == NULL || sbiow == NULL) {
	    sm_end(NOTOK);
	    return sm_ierror("Unable to create BIO endpoints: %s",
			     ERR_error_string(ERR_get_error(), NULL));
	}

	SSL_set_bio(ssl, sbior, sbiow);
	SSL_set_connect_state(ssl);

	/*
	 * Set up a BIO to handle buffering for us
	 */

	io = BIO_new(BIO_f_buffer());

	if (! io) {
	    sm_end(NOTOK);
	    return sm_ierror("Unable to create a buffer BIO: %s",
			     ERR_error_string(ERR_get_error(), NULL));
	}

	ssl_bio = BIO_new(BIO_f_ssl());

	if (! ssl_bio) {
	    sm_end(NOTOK);
	    return sm_ierror("Unable to create a SSL BIO: %s",
			     ERR_error_string(ERR_get_error(), NULL));
	}

	BIO_set_ssl(ssl_bio, ssl, BIO_CLOSE);
	BIO_push(io, ssl_bio);

	/*
	 * Try doing the handshake now
	 */

	if (BIO_do_handshake(io) < 1) {
	    sm_end(NOTOK);
	    return sm_ierror("Unable to negotiate SSL connection: %s",
			     ERR_error_string(ERR_get_error(), NULL));
	}

	if (sm_debug) {
	    SSL_CIPHER *cipher = SSL_get_current_cipher(ssl);
	    printf("SSL negotiation successful: %s(%d) %s\n",
		   SSL_CIPHER_get_name(cipher),
		   SSL_CIPHER_get_bits(cipher, NULL),
		   SSL_CIPHER_get_version(cipher));

	}

	tls_active = 1;

	doingEHLO = 1;
	result = smtalk (SM_HELO, "EHLO %s", client);
	doingEHLO = 0;

	if (result != 250) {
	    sm_end (NOTOK);
	    return RP_RPLY;
	}
    }
#else  /* TLS_SUPPORT */
    NMH_UNUSED (tls);
#endif /* TLS_SUPPORT */

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
               int debug, int onex, int queued,
               int sasl, char *saslmech, char *user)
{
#ifdef CYRUS_SASL
    char *server_mechs;
#else  /* CYRUS_SASL */
    NMH_UNUSED (server);
    NMH_UNUSED (sasl);
    NMH_UNUSED (saslmech);
    NMH_UNUSED (user);
#endif /* CYRUS_SASL */
    unsigned int i, result, vecp;
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

    /*
     * Last-ditch check just in case client still isn't set to anything
     */

    if (client == NULL || *client == '\0')
	client = "localhost";

#if defined(CYRUS_SASL) || defined(TLS_SUPPORT)
    sasl_inbuffer = malloc(SASL_MAXRECVBUF);
    if (!sasl_inbuffer)
	return sm_ierror("Unable to allocate %d bytes for read buffer",
			 SASL_MAXRECVBUF);
#endif /* CYRUS_SASL || TLS_SUPPORT */

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
	    vec[vecp++] = watch ? "-odi" : queued ? "-odq" : "-odb";
	    vec[vecp++] = "-oem";
	    vec[vecp++] = "-om";
# ifndef RAND
	    if (verbose)
		vec[vecp++] = "-ov";
# endif /* not RAND */
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

	    if (onex)
		smtalk (SM_HELO, "ONEX");
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
sm_winit (int mode, char *from)
{
    char *smtpcom = NULL;

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

        default:
            /* Hopefully, we do not get here. */
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
	    sm_note.length = sm_reply.length;
	    memcpy (sm_note.text, sm_reply.text, sm_reply.length + 1);/* fall */
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
		sm_reply.length = sm_note.length;
		memcpy (sm_reply.text, sm_note.text, sm_note.length + 1);
	    }
	    break;
    }

#ifdef TLS_SUPPORT
    if (tls_active) {
	BIO_ssl_shutdown(io);
	BIO_free_all(io);
    }
#endif /* TLS_SUPPORT */

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
	if (conn) {
	    sasl_dispose(&conn);
	    if (sasl_outbuffer) {
		free(sasl_outbuffer);
	    }
	}
	if (sasl_inbuffer)
	    free(sasl_inbuffer);
#endif /* CYRUS_SASL */
    } else {
	status = pidwait (sm_child, OK);
	sm_child = NOTOK;
    }

    sm_rfp = sm_wfp = NULL;
    return (status ? RP_BHST : RP_OK);
}

#ifdef CYRUS_SASL
/*
 * This function implements SASL authentication for SMTP.  If this function
 * completes successfully, then authentication is successful and we've
 * (optionally) negotiated a security layer.
 */
static int
sm_auth_sasl(char *user, char *mechlist, char *inhost)
{
    int result, status;
    unsigned int buflen, outlen;
    char *buf, outbuf[BUFSIZ], host[NI_MAXHOST];
    const char *chosen_mech;
    sasl_security_properties_t secprops;
    sasl_ssf_t *ssf;
    int *outbufmax;

    /*
     * Initialize the callback contexts
     */

    if (user == NULL)
	user = getusername();

    callbacks[SM_SASL_N_CB_USER].context = user;
    callbacks[SM_SASL_N_CB_AUTHNAME].context = user;

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

    sasl_pw_context[0] = host;
    sasl_pw_context[1] = user;

    callbacks[SM_SASL_N_CB_PASS].context = sasl_pw_context;

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
    secprops.max_ssf = tls_active ? 0 : UINT_MAX;

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
	status = sasl_encode64(buf, buflen, outbuf, sizeof(outbuf), NULL);
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
				   outbuf, sizeof(outbuf), &outlen);
	
	    if (result != SASL_OK) {
		smtalk(SM_AUTH, "*");
		sm_ierror("SASL base64 decode failed: %s",
			  sasl_errstring(result, NULL, NULL));
		return NOTOK;
	    }
	}

	result = sasl_client_step(conn, outbuf, outlen, NULL,
				  (const char **) &buf, &buflen);

	if (result != SASL_OK && result != SASL_CONTINUE) {
	    smtalk(SM_AUTH, "*");
	    sm_ierror("SASL client negotiation failed: %s",
		      sasl_errstring(result, NULL, NULL));
	    return NOTOK;
	}

	status = sasl_encode64(buf, buflen, outbuf, sizeof(outbuf), NULL);

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
    char *user = (char *) context;

    if (! result || ((id != SASL_CB_USER) && (id != SASL_CB_AUTHNAME)))
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
    NMH_UNUSED (conn);

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
    strcpy((char *) (*psecret)->data, pass);
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
	if (sasl_ssf)
		printf("(sasl-encrypted) ");
	if (tls_active)
		printf("(tls-encrypted) ");
	printf ("=> %s\n", buffer);
	fflush (stdout);
    }

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

    sm_fwrite (buffer, len);
    sm_fputs ("\r\n");
    sm_fflush ();

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
	    sm_fputs ("\r\n");
	lc = '\0';
	return (ferror (sm_wfp) ? sm_werror () : OK);
    }

    for (bp = buffer; len > 0; bp++, len--) {
	switch (*bp) {
	    case '\n': 
		sm_nl = TRUE;
		sm_fputc ('\r');
		break;

	    case '.': 
		if (sm_nl)
		    sm_fputc ('.');/* FALL THROUGH */
	    default: 
		sm_nl = FALSE;
	}
	sm_fputc (*bp);
	if (ferror (sm_wfp))
	    return sm_werror ();
    }

    if (bp > buffer)
	lc = *--bp;
    return (ferror (sm_wfp) ? sm_werror () : OK);
}

/*
 * Write out to the network, but do buffering for SASL (if enabled)
 */

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
	fwrite(buffer, sizeof(*buffer), len, sm_wfp);
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

	    fwrite(output, sizeof(*output), outputlen, sm_wfp);
	}

	if (len > 0) {
	    memcpy(sasl_outbuffer + sasl_outbuflen, buffer, len);
	    sasl_outbuflen += len;
	}
    }
#endif /* CYRUS_SASL */
    return ferror(sm_wfp) ? NOTOK : RP_OK;
}

/*
 * Convenience functions to replace occurences of fputs() and fputc()
 */

static int
sm_fputs(char *buffer)
{
    return sm_fwrite(buffer, strlen(buffer));
}

static int
sm_fputc(int c)
{
    char h = c;

    return sm_fwrite(&h, 1);
}

/*
 * Flush out any pending data on the connection
 */

static void
sm_fflush(void)
{
#ifdef CYRUS_SASL
    const char *output;
    unsigned int outputlen;
    int result;

    if (sasl_complete == 1 && sasl_ssf > 0 && sasl_outbuflen > 0) {
	result = sasl_encode(conn, sasl_outbuffer, sasl_outbuflen,
			     &output, &outputlen);
	if (result != SASL_OK) {
	    sm_ierror("Unable to SASL encode connection data: %s",
		      sasl_errdetail(conn));
	    return;
	}

	fwrite(output, sizeof(*output), outputlen, sm_wfp);
	sasl_outbuflen = 0;
    }
#endif /* CYRUS_SASL */

#ifdef TLS_SUPPORT
    if (tls_active) {
    	(void) BIO_flush(io);
    }
#endif /* TLS_SUPPORT */

    fflush(sm_wfp);
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


static int
smhear (void)
{
    int i, code, cont, bc = 0, rc, more;
    unsigned char *bp;
    char *rp;
    char **ehlo = NULL, buffer[BUFSIZ];

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

    for (more = FALSE; sm_rrecord ((char *) (bp = (unsigned char *) buffer),
				   &bc) != NOTOK ; ) {
	if (sm_debug) {
	    if (sasl_ssf > 0)
		printf("(sasl-decrypted) ");
	    if (tls_active)
		printf("(tls-decrypted) ");
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
	code = atoi ((char *) bp);
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
		/* can never fail to 0-terminate because of size of buffer vs fixed string */
		strncpy (buffer, sm_noreply, sizeof(buffer));
		bp = (unsigned char *) buffer;
		bc = strlen (sm_noreply);
	    }
	}

	if ((i = min (bc, rc)) > 0) {
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
    return NOTOK;
}


static int
sm_rrecord (char *buffer, int *len)
{
    int retval;

    if (sm_rfp == NULL)
	return sm_rerror(0);

    buffer[*len = 0] = 0;

    if ((retval = sm_fgets (buffer, BUFSIZ, sm_rfp)) != RP_OK)
	return retval;
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
