/*
 * popsbr.c -- POP client subroutines
 *
 * $Id$
 */

#include <h/mh.h>

extern int  client(char *args, char *protocol, char *service, int rproto,
		   char *response, int len_response);

#if defined(NNTP) && !defined(PSHSBR)
# undef NNTP
#endif

#ifdef NNTP			/* building pshsbr.o from popsbr.c */
# include <h/nntp.h>
#endif /* NNTP */

#if !defined(NNTP) && defined(APOP)
# include <h/md5.h>
#endif

#ifdef CYRUS_SASL
# include <sasl.h>
#endif /* CYRUS_SASL */

#include <h/popsbr.h>
#include <h/signals.h>
#include <signal.h>
#include <errno.h>

#define	TRM	"."
#define	TRMLEN	(sizeof TRM - 1)

static int poprint = 0;
static int pophack = 0;

char response[BUFSIZ];

static FILE *input;
static FILE *output;

#define	targ_t char *

#if !defined(NNTP) && defined(MPOP)
# define command pop_command
# define multiline pop_multiline
#endif

#ifdef NNTP
# ifdef BPOP	/* stupid */
static int xtnd_last = -1;
static int xtnd_first = 0;
static char xtnd_name[512];	/* INCREDIBLE HACK!! */
# endif
#endif /* NNTP */

#ifdef CYRUS_SASL
static sasl_conn_t *conn;	/* SASL connection state */
static int sasl_complete = 0;	/* Has sasl authentication succeeded? */
static int maxoutbuf;		/* Maximum output buffer size */
static sasl_ssf_t sasl_ssf = 0;	/* Security strength factor */
static int sasl_get_user(void *, int, const char **, unsigned *);
static int sasl_get_pass(sasl_conn_t *, void *, int, sasl_secret_t **);
struct pass_context {
    char *user;
    char *host;
};

static sasl_callback_t callbacks[] = {
    { SASL_CB_USER, sasl_get_user, NULL },
#define POP_SASL_CB_N_USER 0
    { SASL_CB_PASS, sasl_get_pass, NULL },
#define POP_SASL_CB_N_PASS 1
    { SASL_CB_LIST_END, NULL, NULL },
};
#else /* CYRUS_SASL */
# define sasl_fgetc fgetc
#endif /* CYRUS_SASL */

/*
 * static prototypes
 */
#if !defined(NNTP) && defined(APOP)
static char *pop_auth (char *, char *);
#endif

#if defined(NNTP) || !defined(MPOP)
/* otherwise they are not static functions */
static int command(const char *, ...);
static int multiline(void);
#endif

#ifdef CYRUS_SASL
static int pop_auth_sasl(char *, char *, char *);
static int sasl_fgetc(FILE *);
#endif /* CYRUS_SASL */

static int traverse (int (*)(), const char *, ...);
static int vcommand(const char *, va_list);
static int getline (char *, int, FILE *);
static int putline (char *, FILE *);


#if !defined(NNTP) && defined(APOP)
static char *
pop_auth (char *user, char *pass)
{
    int len, buflen;
    char *cp, *lp;
    unsigned char *dp, *ep, digest[16];
    MD5_CTX mdContext;
    static char buffer[BUFSIZ];

    if ((cp = strchr (response, '<')) == NULL
	    || (lp = strchr (cp, '>')) == NULL) {
	snprintf (buffer, sizeof(buffer), "APOP not available: %s", response);
	strncpy (response, buffer, sizeof(response));
	return NULL;
    }

    *(++lp) = '\0';
    snprintf (buffer, sizeof(buffer), "%s%s", cp, pass);

    MD5Init (&mdContext);
    MD5Update (&mdContext, (unsigned char *) buffer,
	       (unsigned int) strlen (buffer));
    MD5Final (digest, &mdContext);

    cp = buffer;
    buflen = sizeof(buffer);

    snprintf (cp, buflen, "%s ", user);
    len = strlen (cp);
    cp += len;
    buflen -= len;

    for (ep = (dp = digest) + sizeof(digest) / sizeof(digest[0]); dp < ep; ) {
	snprintf (cp, buflen, "%02x", *dp++ & 0xff);
	cp += 2;
	buflen -= 2;
    }
    *cp = '\0';

    return buffer;
}
#endif	/* !NNTP && APOP */

#ifdef CYRUS_SASL
/*
 * This function implements the AUTH command for various SASL mechanisms
 *
 * We do the whole SASL dialog here.  If this completes, then we've
 * authenticated successfully and have (possibly) negotiated a security
 * layer.
 */

int
pop_auth_sasl(char *user, char *host, char *mech)
{
    int result, status, sasl_capability = 0, outlen;
    unsigned int buflen;
    char server_mechs[256], *buf, outbuf[BUFSIZ];
    const char *chosen_mech;
    sasl_security_properties_t secprops;
    sasl_external_properties_t extprops;
    struct pass_context p_context;
    sasl_ssf_t *ssf;
    int *moutbuf;

    /*
     * First off, we're going to send the CAPA command to see if we can
     * even support the AUTH command, and if we do, then we'll get a
     * list of mechanisms the server supports.  If we don't support
     * the CAPA command, then it's unlikely that we will support
     * SASL
     */

    if (command("CAPA") == NOTOK) {
	snprintf(response, sizeof(response),
		 "The POP CAPA command failed; POP server does not "
		 "support SASL");
	return NOTOK;
    }

    while ((status = multiline()) != DONE)
	switch (status) {
	case NOTOK:
	    return NOTOK;
	    break;
	case DONE:	/* Shouldn't be possible, but just in case */
	    break;
	case OK:
	    if (strncasecmp(response, "SASL ", 5) == 0) {
		/*
		 * We've seen the SASL capability.  Grab the mech list
		 */
		sasl_capability++;
		strncpy(server_mechs, response + 5, sizeof(server_mechs));
	    }
	    break;
	}

    if (!sasl_capability) {
	snprintf(response, sizeof(response), "POP server does not support "
		 "SASL");
	return NOTOK;
    }

    /*
     * If we received a preferred mechanism, see if the server supports it.
     */

    if (mech && stringdex(mech, server_mechs) == -1) {
	snprintf(response, sizeof(response), "Requested SASL mech \"%s\" is "
		 "not in list of supported mechanisms:\n%s",
		 mech, server_mechs);
	return NOTOK;
    }

    /*
     * Start the SASL process.  First off, initialize the SASL library.
     */

    callbacks[POP_SASL_CB_N_USER].context = user;
    p_context.user = user;
    p_context.host = host;
    callbacks[POP_SASL_CB_N_PASS].context = &p_context;

    result = sasl_client_init(callbacks);

    if (result != SASL_OK) {
	snprintf(response, sizeof(response), "SASL library initialization "
		 "failed: %s", sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    result = sasl_client_new("pop", host, NULL, SASL_SECURITY_LAYER, &conn);

    if (result != SASL_OK) {
	snprintf(response, sizeof(response), "SASL client initialization "
		 "failed: %s", sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    /*
     * Initialize the security properties
     */

    memset(&secprops, 0, sizeof(secprops));
    secprops.maxbufsize = BUFSIZ;
    secprops.max_ssf = UINT_MAX;
    memset(&extprops, 0, sizeof(extprops));

    result = sasl_setprop(conn, SASL_SEC_PROPS, &secprops);

    if (result != SASL_OK) {
	snprintf(response, sizeof(response), "SASL security property "
		 "initialization failed: %s",
		 sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    result = sasl_setprop(conn, SASL_SSF_EXTERNAL, &extprops);

    if (result != SASL_OK) {
	snprintf(response, sizeof(response), "SASL external property "
		 "initialization failed: %s",
		 sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    /*
     * Start the actual protocol.  Feed the mech list into the library
     * and get out a possible initial challenge
     */

    result = sasl_client_start(conn, mech ? mech : server_mechs,
			       NULL, NULL, &buf, &buflen, &chosen_mech);

    if (result != SASL_OK && result != SASL_CONTINUE) {
	snprintf(response, sizeof(response), "SASL client start failed: %s",
		 sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    if (buflen) {
	status = sasl_encode64(buf, buflen, outbuf, sizeof(outbuf), NULL);
	free(buf);
	if (status != SASL_OK) {
	    snprintf(response, sizeof(response), "SASL base64 encode "
		     "failed: %s", sasl_errstring(status, NULL, NULL));
	    return NOTOK;
	}

	status = command("AUTH %s %s", chosen_mech, outbuf);
    } else
	status = command("AUTH %s", chosen_mech);

    while (result == SASL_CONTINUE) {
	if (status == NOTOK)
	    return NOTOK;
	
	/*
	 * If we get a "+OK" prefix to our response, then we should
	 * exit out of this exchange now (because authenticated should
	 * have succeeded)
	 */
	
	if (strncmp(response, "+OK", 3) == 0)
	    break;
	
	/*
	 * Otherwise, make sure the server challenge is correctly formatted
	 */
	
	if (strncmp(response, "+ ", 2) != 0) {
	    command("*");
	    snprintf(response, sizeof(response),
		     "Malformed authentication message from server");
	    return NOTOK;
	}

	result = sasl_decode64(response + 2, strlen(response + 2),
			       outbuf, &outlen);
	
	if (result != SASL_OK) {
	    command("*");
	    snprintf(response, sizeof(response), "SASL base64 decode "
		     "failed: %s", sasl_errstring(result, NULL, NULL));
	    return NOTOK;
	}

	result = sasl_client_step(conn, outbuf, outlen, NULL, &buf, &buflen);

	if (result != SASL_OK && result != SASL_CONTINUE) {
	    command("*");
	    snprintf(response, sizeof(response), "SASL client negotiaton "
		     "failed: %s", sasl_errstring(result, NULL, NULL));
	    return NOTOK;
	}

	status = sasl_encode64(buf, buflen, outbuf, sizeof(outbuf), NULL);
	free(buf);

	if (status != SASL_OK) {
	    command("*");
	    snprintf(response, sizeof(response), "SASL base64 encode "
		     "failed: %s", sasl_errstring(status, NULL, NULL));
	    return NOTOK;
	}

	status = command(outbuf);
    }

    /*
     * If we didn't get a positive final response, then error out
     * (that probably means we failed an authorization check).
     */

    if (status != OK)
	return NOTOK;

    /*
     * Depending on the mechanism, we might need to call sasl_client_step()
     * one more time.  Do that now.
     */

    result = sasl_client_step(conn, NULL, 0, NULL, &buf, &buflen);

    if (result != SASL_OK) {
	snprintf(response, sizeof(response), "SASL final client negotiaton "
		 "failed: %s", sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    /*
     * We _should_ be okay now.  Get a few properties now that negotiation
     * has completed.
     */

    result = sasl_getprop(conn, SASL_MAXOUTBUF, (void **) &moutbuf);

    if (result != SASL_OK) {
	snprintf(response, sizeof(response), "Cannot retrieve SASL negotiated "
		 "output buffer size: %s", sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    maxoutbuf = *moutbuf;

    result = sasl_getprop(conn, SASL_SSF, (void **) &ssf);

    sasl_ssf = *ssf;

    if (result != SASL_OK) {
	snprintf(response, sizeof(response), "Cannot retrieve SASL negotiated "
		 "security strength factor: %s",
		 sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    /*
     * Limit this to what we can deal with.
     */

    if (maxoutbuf == 0 || maxoutbuf > BUFSIZ)
	maxoutbuf = BUFSIZ;

    sasl_complete = 1;

    return status;
}

/*
 * Callback to return the userid sent down via the user parameter
 */

static int
sasl_get_user(void *context, int id, const char **result, unsigned *len)
{
    char *user = (char *) context;

    if (! result || id != SASL_CB_USER)
	return SASL_BADPARAM;

    *result = user;
    if (len)
	*len = strlen(user);

    return SASL_OK;
}

/*
 * Callback to return the password (we call ruserpass, which can get it
 * out of the .netrc
 */

static int
sasl_get_pass(sasl_conn_t *conn, void *context, int id, sasl_secret_t **psecret)
{
    struct pass_context *p_context = (struct pass_context *) context;
    char *pass = NULL;
    int len;

    if (! psecret || id != SASL_CB_PASS)
	return SASL_BADPARAM;

    ruserpass(p_context->user, &(p_context->host), &pass);

    len = strlen(pass);

    *psecret = (sasl_secret_t *) malloc(sizeof(sasl_secret_t) + len);

    if (! *psecret)
	return SASL_NOMEM;

    (*psecret)->len = len;
    strcpy((*psecret)->data, pass);

    return SASL_OK;
}
#endif /* CYRUS_SASL */

int
pop_init (char *host, char *user, char *pass, int snoop, int rpop, int kpop,
	  int sasl, char *mech)
{
    int fd1, fd2;
    char buffer[BUFSIZ];

#ifdef APOP
    int apop;

    if ((apop = rpop) < 0)
	rpop = 0;
#endif

#ifndef NNTP
# ifdef KPOP
    if ( kpop ) {
	snprintf (buffer, sizeof(buffer), "%s/%s", KPOP_PRINCIPAL, "kpop");
	if ((fd1 = client (host, "tcp", buffer, 0, response, sizeof(response))) == NOTOK) {
	    return NOTOK;
	}
    } else {
# endif /* KPOP */
      if ((fd1 = client (host, "tcp", POPSERVICE, rpop, response, sizeof(response))) == NOTOK) {
	    return NOTOK;
      }
# ifdef KPOP
   }
# endif /* KPOP */
#else	/* NNTP */
    if ((fd1 = client (host, "tcp", "nntp", rpop, response, sizeof(response))) == NOTOK)
	return NOTOK;
#endif

    if ((fd2 = dup (fd1)) == NOTOK) {
	char *s;

	if ((s = strerror(errno)))
	    snprintf (response, sizeof(response),
		"unable to dup connection descriptor: %s", s);
	else
	    snprintf (response, sizeof(response),
		"unable to dup connection descriptor: unknown error");
	close (fd1);
	return NOTOK;
    }
#ifndef NNTP
    if (pop_set (fd1, fd2, snoop) == NOTOK)
#else	/* NNTP */
    if (pop_set (fd1, fd2, snoop, (char *)0) == NOTOK)
#endif	/* NNTP */
	return NOTOK;

    SIGNAL (SIGPIPE, SIG_IGN);

    switch (getline (response, sizeof response, input)) {
	case OK: 
	    if (poprint)
		fprintf (stderr, "<--- %s\n", response);
#ifndef	NNTP
	    if (*response == '+') {
# ifndef KPOP
#  ifdef APOP
		if (apop < 0) {
		    char *cp = pop_auth (user, pass);

		    if (cp && command ("APOP %s", cp) != NOTOK)
			return OK;
		}
		else
#  endif /* APOP */
#  ifdef CYRUS_SASL
		if (sasl) {
		    if (pop_auth_sasl(user, host, mech) != NOTOK)
			return OK;
		} else
#  endif /* CYRUS_SASL */
		if (command ("USER %s", user) != NOTOK
		    && command ("%s %s", rpop ? "RPOP" : (pophack++, "PASS"),
					pass) != NOTOK)
		return OK;
# else /* KPOP */
		if (command ("USER %s", user) != NOTOK
		    && command ("PASS %s", pass) != NOTOK)
		return OK;
# endif
	    }
#else /* NNTP */
	    if (*response < CHAR_ERR) {
		command ("MODE READER");
		return OK;
	    }
#endif
	    strncpy (buffer, response, sizeof(buffer));
	    command ("QUIT");
	    strncpy (response, buffer, sizeof(response));
				/* and fall */

	case NOTOK: 
	case DONE: 
	    if (poprint)	    
		fprintf (stderr, "%s\n", response);
	    fclose (input);
	    fclose (output);
	    return NOTOK;
    }

    return NOTOK;	/* NOTREACHED */
}

#ifdef NNTP
int
pop_set (int in, int out, int snoop, char *myname)
#else
int
pop_set (int in, int out, int snoop)
#endif
{

#ifdef NNTP
    if (myname && *myname) {
	/* interface from bbc to msh */
	strncpy (xtnd_name, myname, sizeof(xtnd_name));
    }
#endif	/* NNTP */

    if ((input = fdopen (in, "r")) == NULL
	    || (output = fdopen (out, "w")) == NULL) {
	strncpy (response, "fdopen failed on connection descriptor", sizeof(response));
	if (input)
	    fclose (input);
	else
	    close (in);
	close (out);
	return NOTOK;
    }

    poprint = snoop;

    return OK;
}


int
pop_fd (char *in, int inlen, char *out, int outlen)
{
    snprintf (in, inlen, "%d", fileno (input));
    snprintf (out, outlen, "%d", fileno (output));
    return OK;
}


/*
 * Find out number of messages available
 * and their total size.
 */

int
pop_stat (int *nmsgs, int *nbytes)
{
#ifdef NNTP
    char **ap;
#endif /* NNTP */

#ifndef	NNTP
    if (command ("STAT") == NOTOK)
	return NOTOK;

    *nmsgs = *nbytes = 0;
    sscanf (response, "+OK %d %d", nmsgs, nbytes);

#else /* NNTP */
    if (xtnd_last < 0) { 	/* in msh, xtnd_name is set from myname */
	if (command("GROUP %s", xtnd_name) == NOTOK)
	    return NOTOK;

	ap = brkstring (response, " ", "\n"); /* "211 nart first last ggg" */
	xtnd_first = atoi (ap[2]);
	xtnd_last  = atoi (ap[3]);
    }

    /* nmsgs is not the real nart, but an incredible simuation */
    if (xtnd_last > 0)
	*nmsgs = xtnd_last - xtnd_first + 1;	/* because of holes... */
    else
	*nmsgs = 0;
    *nbytes = xtnd_first;	/* for subtracting offset in msh() */
#endif /* NNTP */

    return OK;
}

#ifdef NNTP
int
pop_exists (int (*action)())
{
#ifdef XMSGS		/* hacked into NNTP 1.5 */
    if (traverse (action, "XMSGS %d-%d", (targ_t) xtnd_first, (targ_t) xtnd_last) == OK)
	return OK;
#endif
    /* provided by INN 1.4 */
    if (traverse (action, "LISTGROUP") == OK)
	return OK;
    return traverse (action, "XHDR NONAME %d-%d", (targ_t) xtnd_first, (targ_t) xtnd_last);
}
#endif	/* NNTP */


#ifdef BPOP
int
pop_list (int msgno, int *nmsgs, int *msgs, int *bytes, int *ids)
#else
int
pop_list (int msgno, int *nmsgs, int *msgs, int *bytes)
#endif
{
    int i;
#ifndef	BPOP
    int *ids = NULL;
#endif

    if (msgno) {
#ifndef NNTP
	if (command ("LIST %d", msgno) == NOTOK)
	    return NOTOK;
	*msgs = *bytes = 0;
	if (ids) {
	    *ids = 0;
	    sscanf (response, "+OK %d %d %d", msgs, bytes, ids);
	}
	else
	    sscanf (response, "+OK %d %d", msgs, bytes);
#else /* NNTP */
	*msgs = *bytes = 0;
	if (command ("STAT %d", msgno) == NOTOK) 
	    return NOTOK;
	if (ids) {
	    *ids = msgno;
	}
#endif /* NNTP */
	return OK;
    }

#ifndef NNTP
    if (command ("LIST") == NOTOK)
	return NOTOK;

    for (i = 0; i < *nmsgs; i++)
	switch (multiline ()) {
	    case NOTOK: 
		return NOTOK;
	    case DONE: 
		*nmsgs = ++i;
		return OK;
	    case OK: 
		*msgs = *bytes = 0;
		if (ids) {
		    *ids = 0;
		    sscanf (response, "%d %d %d",
			    msgs++, bytes++, ids++);
		}
		else
		    sscanf (response, "%d %d", msgs++, bytes++);
		break;
	}
    for (;;)
	switch (multiline ()) {
	    case NOTOK: 
		return NOTOK;
	    case DONE: 
		return OK;
	    case OK: 
		break;
	}
#else /* NNTP */
    return NOTOK;
#endif /* NNTP */
}


int
pop_retr (int msgno, int (*action)())
{
#ifndef NNTP
    return traverse (action, "RETR %d", (targ_t) msgno);
#else /* NNTP */
    return traverse (action, "ARTICLE %d", (targ_t) msgno);
#endif /* NNTP */
}


static int
traverse (int (*action)(), const char *fmt, ...)
{
    int result;
    va_list ap;
    char buffer[sizeof(response)];

    va_start(ap, fmt);
    result = vcommand (fmt, ap);
    va_end(ap);

    if (result == NOTOK)
	return NOTOK;
    strncpy (buffer, response, sizeof(buffer));

    for (;;)
	switch (multiline ()) {
	    case NOTOK: 
		return NOTOK;

	    case DONE: 
		strncpy (response, buffer, sizeof(response));
		return OK;

	    case OK: 
		(*action) (response);
		break;
	}
}


int
pop_dele (int msgno)
{
    return command ("DELE %d", msgno);
}


int
pop_noop (void)
{
    return command ("NOOP");
}


#if defined(MPOP) && !defined(NNTP)
int
pop_last (void)
{
    return command ("LAST");
}
#endif


int
pop_rset (void)
{
    return command ("RSET");
}


int
pop_top (int msgno, int lines, int (*action)())
{
#ifndef NNTP
    return traverse (action, "TOP %d %d", (targ_t) msgno, (targ_t) lines);
#else	/* NNTP */
    return traverse (action, "HEAD %d", (targ_t) msgno);
#endif /* NNTP */
}


#ifdef BPOP
int
pop_xtnd (int (*action)(), char *fmt, ...)
{
    int result;
    va_list ap;
    char buffer[BUFSIZ];

#ifdef NNTP
    char **ap;
#endif

    va_start(ap, fmt);
#ifndef NNTP
    /* needs to be fixed... va_end needs to be added */
    snprintf (buffer, sizeof(buffer), "XTND %s", fmt);
    result = traverse (action, buffer, a, b, c, d);
    va_end(ap);
    return result;
#else /* NNTP */
    snprintf (buffer, sizeof(buffer), fmt, a, b, c, d);
    ap = brkstring (buffer, " ", "\n");	/* a hack, i know... */

    if (!strcasecmp(ap[0], "x-bboards")) {	/* XTND "X-BBOARDS group */
	/* most of these parameters are meaningless under NNTP. 
	 * bbc.c was modified to set AKA and LEADERS as appropriate,
	 * the rest are left blank.
	 */
	return OK;
    }
    if (!strcasecmp (ap[0], "archive") && ap[1]) {
	snprintf (xtnd_name, sizeof(xtnd_name), "%s", ap[1]);	/* save the name */
	xtnd_last = 0;
	xtnd_first = 1;		/* setup to fail in pop_stat */
	return OK;
    }
    if (!strcasecmp (ap[0], "bboards")) {

	if (ap[1]) {			/* XTND "BBOARDS group" */
	    snprintf (xtnd_name, sizeof(xtnd_name), "%s", ap[1]);	/* save the name */
	    if (command("GROUP %s", xtnd_name) == NOTOK)
		return NOTOK;

	    /* action must ignore extra args */
	    strncpy (buffer, response, sizeof(buffer));
	    ap = brkstring (response, " ", "\n");/* "211 nart first last g" */
	    xtnd_first = atoi (ap[2]);
	    xtnd_last  = atoi (ap[3]);

	    (*action) (buffer);		
	    return OK;

	} else {		/* XTND "BBOARDS" */
	    return traverse (action, "LIST", a, b, c, d);
	}
    }
    return NOTOK;	/* unknown XTND command */
#endif /* NNTP */
}
#endif BPOP


int
pop_quit (void)
{
    int i;

    i = command ("QUIT");
    pop_done ();

    return i;
}


int
pop_done (void)
{
#ifdef CYRUS_SASL
    if (conn)
	sasl_dispose(&conn);
#endif /* CYRUS_SASL */
    fclose (input);
    fclose (output);

    return OK;
}


#if !defined(MPOP) || defined(NNTP)
static
#endif
int
command(const char *fmt, ...)
{
    va_list ap;
    int result;

    va_start(ap, fmt);
    result = vcommand(fmt, ap);
    va_end(ap);

    return result;
}


static int
vcommand (const char *fmt, va_list ap)
{
    char *cp, buffer[BUFSIZ];

    vsnprintf (buffer, sizeof(buffer), fmt, ap);
    if (poprint) {
#ifdef CYRUS_SASL
	if (sasl_ssf)
	    fprintf(stderr, "(encrypted) ");
#endif /* CYRUS_SASL */
	if (pophack) {
	    if ((cp = strchr (buffer, ' ')))
		*cp = 0;
	    fprintf (stderr, "---> %s ********\n", buffer);
	    if (cp)
		*cp = ' ';
	    pophack = 0;
	}
	else
	    fprintf (stderr, "---> %s\n", buffer);
    }

    if (putline (buffer, output) == NOTOK)
	return NOTOK;

#ifdef CYRUS_SASL
    if (poprint && sasl_ssf)
	fprintf(stderr, "(decrypted) ");
#endif /* CYRUS_SASL */

    switch (getline (response, sizeof response, input)) {
	case OK: 
	    if (poprint)
		fprintf (stderr, "<--- %s\n", response);
#ifndef NNTP
	    return (*response == '+' ? OK : NOTOK);
#else	/* NNTP */
	    return (*response < CHAR_ERR ? OK : NOTOK);
#endif	/* NNTP */

	case NOTOK: 
	case DONE: 
	    if (poprint)	    
		fprintf (stderr, "%s\n", response);
	    return NOTOK;
    }

    return NOTOK;	/* NOTREACHED */
}


#if defined(MPOP) && !defined(NNTP)
int
multiline (void)
#else
static int
multiline (void)
#endif
{
    char buffer[BUFSIZ + TRMLEN];

    if (getline (buffer, sizeof buffer, input) != OK)
	return NOTOK;
#ifdef DEBUG
    if (poprint) {
#ifdef CYRUS_SASL
	if (sasl_ssf)
	    fprintf(stderr, "(decrypted) ");
#endif /* CYRUS_SASL */
	fprintf (stderr, "<--- %s\n", response);
    }
#endif DEBUG
    if (strncmp (buffer, TRM, TRMLEN) == 0) {
	if (buffer[TRMLEN] == 0)
	    return DONE;
	else
	    strncpy (response, buffer + TRMLEN, sizeof(response));
    }
    else
	strncpy (response, buffer, sizeof(response));

    return OK;
}

/*
 * Note that these functions have been modified to deal with layer encryption
 * in the SASL case
 */

static int
getline (char *s, int n, FILE *iop)
{
    int c;
    char *p;

    p = s;
    while (--n > 0 && (c = sasl_fgetc (iop)) != EOF && c != -2) 
	if ((*p++ = c) == '\n')
	    break;
    if (c == -2)
	return NOTOK;
    if (ferror (iop) && c != EOF) {
	strncpy (response, "error on connection", sizeof(response));
	return NOTOK;
    }
    if (c == EOF && p == s) {
	strncpy (response, "connection closed by foreign host", sizeof(response));
	return DONE;
    }
    *p = 0;
    if (*--p == '\n')
	*p = 0;
    if (*--p == '\r')
	*p = 0;

    return OK;
}


static int
putline (char *s, FILE *iop)
{
#ifdef CYRUS_SASL
    char outbuf[BUFSIZ], *buf;
    int result;
    unsigned int buflen;

    if (!sasl_complete) {
#endif /* CYRUS_SASL */
	fprintf (iop, "%s\r\n", s);
#ifdef CYRUS_SASL
    } else {
	/*
	 * Build an output buffer, encrypt it using sasl_encode, and
	 * squirt out the results.
	 */
	strncpy(outbuf, s, sizeof(outbuf) - 3);
	outbuf[sizeof(outbuf) - 3] = '\0';   /* Just in case */
	strcat(outbuf, "\r\n");

	result = sasl_encode(conn, outbuf, strlen(outbuf), &buf, &buflen);

	if (result != SASL_OK) {
	    snprintf(response, sizeof(response), "SASL encoding error: %s",
		     sasl_errstring(result, NULL, NULL));
	    return NOTOK;
	}

	fwrite(buf, buflen, 1, iop);
	free(buf);
    }
#endif /* CYRUS_SASL */

    fflush (iop);
    if (ferror (iop)) {
	strncpy (response, "lost connection", sizeof(response));
	return NOTOK;
    }

    return OK;
}

#ifdef CYRUS_SASL
/*
 * Okay, our little fgetc replacement.  Hopefully this is a little more
 * efficient than the last one.
 */
static int
sasl_fgetc(FILE *f)
{
    static unsigned char *buffer = NULL, *ptr;
    static int size = 0;
    static int cnt = 0;
    unsigned int retbufsize = 0;
    int cc, result;
    char *retbuf, tmpbuf[BUFSIZ];

    /*
     * If we have some leftover data, return that
     */

    if (cnt) {
	cnt--;
	return (int) *ptr++;
    }

    /*
     * Otherwise, fill our buffer until we have some data to return.
     */

    while (retbufsize == 0) {

	cc = read(fileno(f), tmpbuf, sizeof(tmpbuf));

	if (cc == 0)
	    return EOF;

	if (cc < 0) {
	    snprintf(response, sizeof(response), "Error during read from "
		     "network: %s", strerror(errno));
	    return -2;
	}

	/*
	 * We're not allowed to call sasl_decode until sasl_complete is
	 * true, so we do these gyrations ...
	 */
	
	if (!sasl_complete) {

	    retbuf = tmpbuf;
	    retbufsize = cc;

	} else {

	    result = sasl_decode(conn, tmpbuf, cc, &retbuf, &retbufsize);

	    if (result != SASL_OK) {
		snprintf(response, sizeof(response), "Error during SASL "
			 "decoding: %s", sasl_errstring(result, NULL, NULL));
		return -2;
	    }
	}
    }

    if (retbufsize > size) {
	buffer = realloc(buffer, retbufsize);
	if (!buffer) {
	    snprintf(response, sizeof(response), "Error during realloc in "
		     "read routine: %s", strerror(errno));
	    return -2;
	}
	size = retbufsize;
    }

    memcpy(buffer, retbuf, retbufsize);
    ptr = buffer + 1;
    cnt = retbufsize - 1;
    if (sasl_complete)
	free(retbuf);

    return (int) buffer[0];
}
#endif /* CYRUS_SASL */
