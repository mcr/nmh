/*
 * popsbr.c -- POP client subroutines
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <h/oauth.h>
#include <h/netsec.h>

#include <h/popsbr.h>
#include <h/signals.h>

#define	TRM	"."
#define	TRMLEN	(sizeof TRM - 1)

static int poprint = 0;
static int pophack = 0;

char response[BUFSIZ];
static netsec_context *nsc = NULL;

/*
 * static prototypes
 */

static int command(const char *, ...);
static int multiline(void);

static int traverse (int (*)(char *), const char *, ...);
static int vcommand(const char *, va_list);
static int getline (char *, int, FILE *);
static int sasl_fgetc(FILE *);
static int putline (char *, FILE *);


int
check_mech(char *server_mechs, size_t server_mechs_size, char *mech)
{
  int status, sasl_capability = 0;

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
		strncpy(server_mechs, response + 5, server_mechs_size);
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

    return OK;
}

#ifdef CYRUS_SASL
/*
 * This function implements the AUTH command for various SASL mechanisms
 *
 * We do the whole SASL dialog here.  If this completes, then we've
 * authenticated successfully and have (possibly) negotiated a security
 * layer.
 */

#define CHECKB64SIZE(insize, outbuf, outsize) \
    { size_t wantout = (((insize + 2) / 3) * 4) + 32; \
      if (wantout > outsize) { \
          outbuf = mh_xrealloc(outbuf, outsize = wantout); \
      } \
    }

int
pop_auth_sasl(char *user, char *host, char *mech)
{
    int result, status;
    unsigned int buflen, outlen;
    char server_mechs[256], *buf, *outbuf = NULL;
    size_t outbufsize = 0;
    const char *chosen_mech;
    sasl_security_properties_t secprops;
    struct pass_context p_context;
    sasl_ssf_t *ssf;
    int *moutbuf;

    if ((status = check_mech(server_mechs, sizeof(server_mechs), mech)) != OK) {
	return status;
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

    result = sasl_client_new("pop", host, NULL, NULL, NULL, 0, &conn);

    if (result != SASL_OK) {
	snprintf(response, sizeof(response), "SASL client initialization "
		 "failed: %s", sasl_errstring(result, NULL, NULL));
	return NOTOK;
    }

    /*
     * Initialize the security properties
     */

    memset(&secprops, 0, sizeof(secprops));
    secprops.maxbufsize = SASL_BUFFER_SIZE;
    secprops.max_ssf = tls_active ? 0 : UINT_MAX;

    result = sasl_setprop(conn, SASL_SEC_PROPS, &secprops);

    if (result != SASL_OK) {
	snprintf(response, sizeof(response), "SASL security property "
		 "initialization failed: %s", sasl_errdetail(conn));
	return NOTOK;
    }

    /*
     * Start the actual protocol.  Feed the mech list into the library
     * and get out a possible initial challenge
     */

    result = sasl_client_start(conn,
			       (const char *) (mech ? mech : server_mechs),
			       NULL, (const char **) &buf,
			       &buflen, &chosen_mech);

    if (result != SASL_OK && result != SASL_CONTINUE) {
	snprintf(response, sizeof(response), "SASL client start failed: %s",
		 sasl_errdetail(conn));
	return NOTOK;
    }

    if (buflen) {
	CHECKB64SIZE(buflen, outbuf, outbufsize);
	status = sasl_encode64(buf, buflen, outbuf, outbufsize, NULL);
	if (status != SASL_OK) {
	    snprintf(response, sizeof(response), "SASL base64 encode "
		     "failed: %s", sasl_errstring(status, NULL, NULL));
	    if (outbuf)
		free(outbuf);
	    return NOTOK;
	}

	status = command("AUTH %s %s", chosen_mech, outbuf);
    } else
	status = command("AUTH %s", chosen_mech);

    while (result == SASL_CONTINUE) {
	size_t inlen;

	if (status == NOTOK) {
	    if (outbuf)
		free(outbuf);
	    return NOTOK;
	}
	
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
	    if (outbuf)
		free(outbuf);
	    return NOTOK;
	}

	/*
	 * For decode, it will always be shorter, so just make sure
	 * that outbuf is as at least as big as the encoded response.
	 */

	inlen = strlen(response + 2);

	if (inlen > outbufsize) {
	    outbuf = mh_xrealloc(outbuf, outbufsize = inlen);
	}

	result = sasl_decode64(response + 2, strlen(response + 2),
			       outbuf, outbufsize, &outlen);
	
	if (result != SASL_OK) {
	    command("*");
	    snprintf(response, sizeof(response), "SASL base64 decode "
		     "failed: %s", sasl_errstring(result, NULL, NULL));
	    if (outbuf)
		free(outbuf);
	    return NOTOK;
	}

	result = sasl_client_step(conn, outbuf, outlen, NULL,
				  (const char **) &buf, &buflen);

	if (result != SASL_OK && result != SASL_CONTINUE) {
	    command("*");
	    snprintf(response, sizeof(response), "SASL client negotiaton "
		     "failed: %s", sasl_errdetail(conn));
	    if (outbuf)
		free(outbuf);
	    return NOTOK;
	}

	CHECKB64SIZE(buflen, outbuf, outbufsize);

	status = sasl_encode64(buf, buflen, outbuf, outbufsize, NULL);

	if (status != SASL_OK) {
	    command("*");
	    snprintf(response, sizeof(response), "SASL base64 encode "
		     "failed: %s", sasl_errstring(status, NULL, NULL));
	    if (outbuf)
		free(outbuf);
	    return NOTOK;
	}

	status = command(outbuf);
    }

    if (outbuf)
	free(outbuf);

    /*
     * If we didn't get a positive final response, then error out
     * (that probably means we failed an authorization check).
     */

    if (status != OK)
	return NOTOK;

    /*
     * We _should_ be okay now.  Get a few properties now that negotiation
     * has completed.
     */

    result = sasl_getprop(conn, SASL_MAXOUTBUF, (const void **) &moutbuf);

    if (result != SASL_OK) {
	snprintf(response, sizeof(response), "Cannot retrieve SASL negotiated "
		 "output buffer size: %s", sasl_errdetail(conn));
	return NOTOK;
    }

    maxoutbuf = *moutbuf;

    result = sasl_getprop(conn, SASL_SSF, (const void **) &ssf);

    sasl_ssf = *ssf;

    if (result != SASL_OK) {
	snprintf(response, sizeof(response), "Cannot retrieve SASL negotiated "
		 "security strength factor: %s", sasl_errdetail(conn));
	return NOTOK;
    }

    /*
     * Limit this to what we can deal with.  Shouldn't matter much because
     * this is only outgoing data (which should be small)
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
    struct nmh_creds creds = { 0, 0, 0 };
    int len;

    NMH_UNUSED (conn);

    if (! psecret || id != SASL_CB_PASS)
	return SASL_BADPARAM;

    if (creds.password == NULL) {
        /*
         * Pass the 0 third argument to nmh_get_credentials() so
         * that the default password isn't used.  With legacy/.netrc
         * credentials support, we'll only get here if the -user
         * switch to send(1)/post(8) wasn't used.
         */
        if (nmh_get_credentials (p_context->host, p_context->user, 0, &creds)
            != OK) {
            return SASL_BADPARAM;
        }
    }

    len = strlen (creds.password);

    *psecret = (sasl_secret_t *) mh_xmalloc(sizeof(sasl_secret_t) + len);

    (*psecret)->len = len;
    strcpy((char *) (*psecret)->data, creds.password);

    return SASL_OK;
}
#endif /* CYRUS_SASL */

int
pop_auth_xoauth(const char *client_res)
{
    char server_mechs[256];
    int status = check_mech(server_mechs, sizeof(server_mechs), "XOAUTH");

    if (status != OK) return status;

    if ((status = command("AUTH XOAUTH2 %s", client_res)) != OK) {
      return status;
    }
    if (strncmp(response, "+OK", 3) == 0) {
	return OK;
    }

    /* response contains base64-encoded JSON, which is always the same.
     * See mts/smtp/smtp.c for more notes on that. */
    /* Then we're supposed to send an empty response ("\r\n"). */
    return command("");
}

/*
 * Split string containing proxy command into an array of arguments
 * suitable for passing to exec. Returned array must be freed. Shouldn't
 * be possible to call this with host set to NULL.
 */
char **
parse_proxy(char *proxy, char *host)
{
    char **pargv, **p;
    int pargc = 2;
    int hlen = strlen(host);
    int plen = 1;
    unsigned char *cur, *pro;
    char *c;
    
    /* skip any initial space */
    for (pro = (unsigned char *) proxy; isspace(*pro); pro++)
        continue;
    
    /* calculate required size for argument array */
    for (cur = pro; *cur; cur++) {
        if (isspace(*cur) && cur[1] && !isspace(cur[1]))
	    plen++, pargc++;
	else if (*cur == '%' && cur[1] == 'h') {
	    plen += hlen;
            cur++;
	} else if (!isspace(*cur))
	    plen++;
    }

   /* put together list of arguments */
    p = pargv = mh_xmalloc(pargc * sizeof(char *));
    c = *pargv = mh_xmalloc(plen * sizeof(char));
    for (cur = pro; *cur; cur++) {
        if (isspace(*cur) && cur[1] && !isspace(cur[1])) {
	    *c++ = '\0';
	    *++p = c;
	} else if (*cur == '%' && cur[1] == 'h') {
	    strcpy (c, host);
	    c += hlen;
	    cur++;
	} else if (!isspace(*cur))
	    *c++ = *cur;
    }
    *c = '\0';
    *++p = NULL;
    return pargv;
}

int
pop_init (char *host, char *port, char *user, char *pass, char *proxy,
	  int snoop, int sasl, char *mech, int tls, const char *oauth_svc)
{
    int fd1;
    char buffer[BUFSIZ];
    char *errstr;
#ifndef CYRUS_SASL
    NMH_UNUSED (sasl);
    NMH_UNUSED (mech);
#endif /* ! CYRUS_SASL */

#ifdef OAUTH_SUPPORT
    if (oauth_svc != NULL) {
	xoauth_client_res = mh_oauth_do_xoauth(user, oauth_svc,
					       snoop ? stderr : NULL);
    }
#else
    NMH_UNUSED (oauth_svc);
    NMH_UNUSED (xoauth_client_res);
#endif /* OAUTH_SUPPORT */

    nsc = netsec_init();  

    if (user)
	netsec_set_userid(nsc, user);

    if (tls) {
	if (netsec_set_tls(nsc, 1, &errstr) != OK) {
	    snprintf(response, sizeof(response), "%s", errstr);
	    free(errstr);
	    return NOTOK;
	}
    }

    if (oauth_svc != NULL) {
	if (netsec_set_oauth_service(nsc, oauth_svc) != OK) {
	    snprintf(response, sizeof(response), "OAuth2 not supported");
	    return NOTOK;
	}
    }

    if (proxy && *proxy) {
       int pid;
       int inpipe[2];	  /* for reading from the server */
       int outpipe[2];    /* for sending to the server */

       if (pipe(inpipe) < 0) {
	   adios ("inpipe", "pipe");
       }
       if (pipe(outpipe) < 0) {
	   adios ("outpipe", "pipe");
       }

       pid=fork();
       if (pid==0) {
	   char **argv;
	   
	   /* in child */
	   close(0);  
	   close(1);
	   dup2(outpipe[0],0);  /* connect read end of connection */
	   dup2(inpipe[1], 1);  /* connect write end of connection */
	   if(inpipe[0]>1) close(inpipe[0]);
	   if(inpipe[1]>1) close(inpipe[1]);
	   if(outpipe[0]>1) close(outpipe[0]);
	   if(outpipe[1]>1) close(outpipe[1]);

	   /* run the proxy command */
	   argv=parse_proxy(proxy, host);
	   execvp(argv[0],argv);

	   perror(argv[0]);
	   close(0);
	   close(1);
	   free(*argv);
	   free(argv);
	   exit(10);
       }

       /* okay in the parent we do some stuff */
       close(inpipe[1]);  /* child uses this */
       close(outpipe[0]); /* child uses this */

       /* we read on fd1 */
       fd1=inpipe[0];
       /* and write on fd2 */
       fd2=outpipe[1];

    } else {
	if ((fd1 = client (host, port ? port : "pop3", response,
			   sizeof(response), snoop)) == NOTOK) {
	    return NOTOK;
	}
    }

    SIGNAL (SIGPIPE, SIG_IGN);

    netsec_set_fd(nsc, fd1);
    netsec_set_snoop(nsc, snoop);

    if (tls) {
	if (netsec_negotiate_tls(nsc, &errstr) != OK) {
	    snprintf(response, sizeof(response), "%s", errstr);
	    free(errstr);
	    return NOTOK;
	}
    }

    if (sasl) {
	if (netsec_set_sasl_params(nsc, host, "pop", mech,
				   pop_sasl_callback, &errstr) != OK) {
	    snprintf(response, sizeof(response), "%s", errstr);
	    free(errstr);
	    return NOTOK;
	}
    }

    switch (getline (response, sizeof response, input)) {
	case OK: 
	    if (poprint)
		fprintf (stderr, "<--- %s\n", response);
	    if (*response == '+') {
#  ifdef CYRUS_SASL
		if (sasl) {
		    if (pop_auth_sasl(user, host, mech) != NOTOK)
			return OK;
		} else
#  endif /* CYRUS_SASL */
#  if OAUTH_SUPPORT
		if (xoauth_client_res != NULL) {
		    if (pop_auth_xoauth(xoauth_client_res) != NOTOK)
			return OK;
		} else
#  endif /* OAUTH_SUPPORT */
		if (command ("USER %s", user) != NOTOK
		    && command ("%s %s", (pophack++, "PASS"),
					pass) != NOTOK)
		return OK;
	    }
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


/*
 * Find out number of messages available
 * and their total size.
 */

int
pop_stat (int *nmsgs, int *nbytes)
{

    if (command ("STAT") == NOTOK)
	return NOTOK;

    *nmsgs = *nbytes = 0;
    sscanf (response, "+OK %d %d", nmsgs, nbytes);

    return OK;
}


int
pop_list (int msgno, int *nmsgs, int *msgs, int *bytes)
{
    int i;
    int *ids = NULL;

    if (msgno) {
	if (command ("LIST %d", msgno) == NOTOK)
	    return NOTOK;
	*msgs = *bytes = 0;
	if (ids) {
	    *ids = 0;
	    sscanf (response, "+OK %d %d %d", msgs, bytes, ids);
	}
	else
	    sscanf (response, "+OK %d %d", msgs, bytes);
	return OK;
    }

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
}


int
pop_retr (int msgno, int (*action)(char *))
{
    return traverse (action, "RETR %d", msgno);
}


static int
traverse (int (*action)(char *), const char *fmt, ...)
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


int
pop_rset (void)
{
    return command ("RSET");
}


int
pop_top (int msgno, int lines, int (*action)(char *))
{
    return traverse (action, "TOP %d %d", msgno, lines);
}


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
    if (nsc)
	netsec_shutdown(nsc, 1);

    return OK;
}


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
    char *cp, buffer[65536];

    vsnprintf (buffer, sizeof(buffer), fmt, ap);

    if (poprint) {
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

    switch (sasl_getline (response, sizeof response, input)) {
	case OK: 
	    if (poprint)
		fprintf (stderr, "<--- %s\n", response);
	    return (*response == '+' ? OK : NOTOK);

	case NOTOK: 
	case DONE: 
	    if (poprint)	    
		fprintf (stderr, "%s\n", response);
	    return NOTOK;
    }

    return NOTOK;	/* NOTREACHED */
}


int
multiline (void)
{
    char buffer[BUFSIZ + TRMLEN];

    if (sasl_getline (buffer, sizeof buffer, input) != OK)
	return NOTOK;
#ifdef DEBUG
    if (poprint) {
#ifdef CYRUS_SASL
	if (sasl_ssf)
	    fprintf(stderr, "(decrypted) ");
#endif /* CYRUS_SASL */
	fprintf (stderr, "<--- %s\n", response);
    }
#endif /* DEBUG */
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
sasl_getline (char *s, int n, FILE *iop)
{
    int c = -2;
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
    if (p > s  &&  *--p == '\r')
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

    if (sasl_complete == 0 || sasl_ssf == 0) {
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
	if (tls_active) {
	    int ret;

	    BIO_printf(io, "%s\r\n");
	    ret = BIO_flush(io);

	    if (ret != 1) {
		strncpy(response, "lost connection", sizeof(response));
		return NOTOK;
	    else {
	    	return OK;
	    }
	} else
#endif /* TLS_SUPPORT */
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

	result = sasl_encode(conn, outbuf, strlen(outbuf),
			     (const char **) &buf, &buflen);

	if (result != SASL_OK) {
	    snprintf(response, sizeof(response), "SASL encoding error: %s",
		     sasl_errdetail(conn));
	    return NOTOK;
	}

	if (fwrite(buf, buflen, 1, iop) < 1) {
	    advise ("putline", "fwrite");
	}
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
    static unsigned int size = 0;
    static int cnt = 0;
    unsigned int retbufsize = 0;
    int cc, result;
    char *retbuf, tmpbuf[SASL_BUFFER_SIZE];

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

#ifdef TLS_SUPPORT
	if (tls_active) {

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

	    result = sasl_decode(conn, tmpbuf, cc,
				 (const char **) &retbuf, &retbufsize);

	    if (result != SASL_OK) {
		snprintf(response, sizeof(response), "Error during SASL "
			 "decoding: %s", sasl_errdetail(conn));
		return -2;
	    }
	}
    }

    if (retbufsize > size) {
	buffer = mh_xrealloc(buffer, retbufsize);
	size = retbufsize;
    }

    memcpy(buffer, retbuf, retbufsize);
    ptr = buffer + 1;
    cnt = retbufsize - 1;

    return (int) buffer[0];
}
#endif /* CYRUS_SASL */
