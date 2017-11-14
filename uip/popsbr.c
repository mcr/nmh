/* popsbr.c -- POP client subroutines
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/credentials.h"
#include "sbr/client.h"
#include "sbr/error.h"
#include "h/utils.h"
#include "h/oauth.h"
#include "h/netsec.h"

#include "popsbr.h"
#include "h/signals.h"
#include "sbr/base64.h"

#define	TRM	"."

static int poprint = 0;

char response[BUFSIZ];
static netsec_context *nsc = NULL;

/*
 * static prototypes
 */

static int command(const char *, ...) CHECK_PRINTF(1, 2);
static int multiline(void);

static int traverse(int (*)(void *, char *), void *closure,
    const char *, ...) CHECK_PRINTF(3, 4);
static int vcommand(const char *, va_list) CHECK_PRINTF(1, 0);
static int pop_getline (char *, int, netsec_context *);
static int pop_sasl_callback(enum sasl_message_type, unsigned const char *,
			     unsigned int, unsigned char **, unsigned int *,
			     void *, char **);

static int
check_mech(char *server_mechs, size_t server_mechs_size)
{
  int status;
  bool sasl_capability = false;

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

    while ((status = multiline()) != DONE) {
        if (status == NOTOK)
	    return NOTOK;

        if (strncasecmp(response, "SASL ", 5) == 0) {
            /* We've seen the SASL capability.  Grab the mech list. */
            sasl_capability = true;
            strncpy(server_mechs, response + 5, server_mechs_size);
        }
    }

    if (!sasl_capability) {
	snprintf(response, sizeof(response), "POP server does not support "
		 "SASL");
	return NOTOK;
    }

    return OK;
}

/*
 * Split string containing proxy command into an array of arguments
 * suitable for passing to exec. Returned array must be freed. Shouldn't
 * be possible to call this with host set to NULL.
 */
static char **
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
    c = *pargv = mh_xmalloc(plen);
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
pop_init (char *host, char *port, char *user, char *proxy, int snoop,
	  int sasl, char *mech, int tls, const char *oauth_svc)
{
    int fd1, fd2;
    char buffer[BUFSIZ];
    char *errstr;

    nsc = netsec_init();  

    if (user)
	netsec_set_userid(nsc, user);

    netsec_set_hostname(nsc, host);

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
	   exit(1);
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
	fd2 = fd1;
    }

    SIGNAL (SIGPIPE, SIG_IGN);

    netsec_set_fd(nsc, fd1, fd2);
    netsec_set_snoop(nsc, snoop);

    if (tls & P_INITTLS) {
	if (netsec_set_tls(nsc, 1, tls & P_NOVERIFY, &errstr) != OK) {
	    snprintf(response, sizeof(response), "%s", errstr);
	    free(errstr);
	    return NOTOK;
	}

	if (netsec_negotiate_tls(nsc, &errstr) != OK) {
	    snprintf(response, sizeof(response), "%s", errstr);
	    free(errstr);
	    return NOTOK;
	}
    }

    if (sasl) {
	if (netsec_set_sasl_params(nsc, "pop", mech, pop_sasl_callback,
				   NULL, &errstr) != OK) {
	    snprintf(response, sizeof(response), "%s", errstr);
	    free(errstr);
	    return NOTOK;
	}
    }

    switch (pop_getline (response, sizeof response, nsc)) {
	case OK: 
	    if (poprint)
		fprintf (stderr, "<--- %s\n", response);
	    if (*response == '+') {
                nmh_creds_t creds;

		if (sasl) {
		    char server_mechs[256];
		    if (check_mech(server_mechs, sizeof(server_mechs)) != OK)
			return NOTOK;
		    if (netsec_negotiate_sasl(nsc, server_mechs,
		    			      &errstr) != OK) {
			strncpy(response, errstr, sizeof(response));
			response[sizeof(response) - 1] = '\0';
			free(errstr);
			return NOTOK;
		    }
		    return OK;
		}

                if (!(creds = nmh_get_credentials(host, user)))
                    return NOTOK;
                if (command ("USER %s", nmh_cred_get_user(creds))
                                                            != NOTOK) {
                    if (command("PASS %s", nmh_cred_get_password(creds))
                                                            != NOTOK) {
                        nmh_credentials_free(creds);
                        return OK;
                    }
                }
                nmh_credentials_free(creds);
	    }
	    strncpy (buffer, response, sizeof(buffer));
	    command ("QUIT");
	    strncpy (response, buffer, sizeof(response));
	    /* FALLTHRU */

	case NOTOK: 
	case DONE: 
	    if (poprint) {
		fputs(response, stderr);
                putc('\n', stderr);
            }
	    netsec_shutdown(nsc);
	    nsc = NULL;
	    return NOTOK;
    }

    return NOTOK;	/* NOTREACHED */
}


/*
 * Our SASL callback; we are given SASL tokens and then have to format
 * them according to the protocol requirements, and then process incoming
 * messages and feed them back into the SASL library.
 */

static int
pop_sasl_callback(enum sasl_message_type mtype, unsigned const char *indata,
		  unsigned int indatalen, unsigned char **outdata,
		  unsigned int *outdatalen, void *context, char **errstr)
{
    int rc, snoopoffset;
    char *mech, *line;
    size_t len, b64len;
    NMH_UNUSED(context);

    switch (mtype) {
    case NETSEC_SASL_START:
	/*
	 * Generate our AUTH message, but there is a wrinkle.
	 *
	 * Technically, according to RFC 5034, if your command INCLUDING
	 * an initial response exceeds 255 octets (including CRLF), you
	 * can't issue this all in one go, but have to just issue the
	 * AUTH command, wait for a blank initial response, and then
	 * send your data.
	 */

	mech = netsec_get_sasl_mechanism(nsc);

	if (indatalen) {
	    char *b64data;
	    b64data = mh_xmalloc(BASE64SIZE(indatalen));
	    writeBase64raw(indata, indatalen, (unsigned char *) b64data);
	    b64len = strlen(b64data);

	    /* Formula here is AUTH + SP + mech + SP + out + CR + LF */
	    len = b64len + 8 + strlen(mech);
	    if (len > 255) {
		rc = netsec_printf(nsc, errstr, "AUTH %s\r\n", mech);
		if (rc)
		    return NOTOK;
		if (netsec_flush(nsc, errstr) != OK)
		    return NOTOK;
		line = netsec_readline(nsc, &len, errstr);
		if (! line)
		    return NOTOK;
		/*
		 * If the protocol is being followed correctly, should just
		 * be a "+ ", nothing else.
		 */
		if (len != 2 || strcmp(line, "+ ") != 0) {
		    netsec_err(errstr, "Did not get expected blank response "
			       "for initial challenge response");
		    return NOTOK;
		}
		rc = netsec_printf(nsc, errstr, "%s\r\n", b64data);
		free(b64data);
		if (rc != OK)
		    return NOTOK;
		netsec_set_snoop_callback(nsc, netsec_b64_snoop_decoder, NULL);
		rc = netsec_flush(nsc, errstr);
		netsec_set_snoop_callback(nsc, NULL, NULL);
		if (rc != OK)
		    return NOTOK;
	    } else {
	        rc = netsec_printf(nsc, errstr, "AUTH %s %s\r\n", mech,
				   b64data);
		free(b64data);
		if (rc != OK)
		    return NOTOK;
		netsec_set_snoop_callback(nsc, netsec_b64_snoop_decoder,
					  &snoopoffset);
		snoopoffset = 6 + strlen(mech);
		rc = netsec_flush(nsc, errstr);
		netsec_set_snoop_callback(nsc, NULL, NULL);
		if (rc != OK)
		    return NOTOK;
	    }
	} else {
	    if (netsec_printf(nsc, errstr, "AUTH %s\r\n", mech) != OK)
		return NOTOK;
	    if (netsec_flush(nsc, errstr) != OK)
		return NOTOK;
	}

	break;

	/*
	 * We should get one line back, with our base64 data.  Decode that
	 * and feed it back into the SASL library.
	 */
    case NETSEC_SASL_READ:
	netsec_set_snoop_callback(nsc, netsec_b64_snoop_decoder, &snoopoffset);
	snoopoffset = 2;
	line = netsec_readline(nsc, &len, errstr);
	netsec_set_snoop_callback(nsc, NULL, NULL);

	if (line == NULL)
	    return NOTOK;
	if (len < 2 || (len == 2 && strcmp(line, "+ ") != 0)) {
	    netsec_err(errstr, "Invalid format for SASL response");
	    return NOTOK;
	}

	if (len == 2) {
	    *outdata = NULL;
	    *outdatalen = 0;
	} else {
	    rc = decodeBase64(line + 2, outdata, &len, 0, NULL);
	    *outdatalen = len;
	    if (rc != OK) {
		netsec_err(errstr, "Unable to decode base64 response");
		return NOTOK;
	    }
	}
	break;

    /*
     * Our encoding is pretty simple, so this is easy.
     */

    case NETSEC_SASL_WRITE:
	if (indatalen == 0) {
	    rc = netsec_printf(nsc, errstr, "\r\n");
	} else {
	    unsigned char *b64data;
	    b64data = mh_xmalloc(BASE64SIZE(indatalen));
	    writeBase64raw(indata, indatalen, b64data);
	    rc = netsec_printf(nsc, errstr, "%s\r\n", b64data);
	    free(b64data);
	}

	if (rc != OK)
	    return NOTOK;

	if (indatalen > 0)
	    netsec_set_snoop_callback(nsc, netsec_b64_snoop_decoder, NULL);
	rc = netsec_flush(nsc, errstr);
	netsec_set_snoop_callback(nsc, NULL, NULL);
	if (rc != OK)
	    return NOTOK;
	break;

    /*
     * Finish the protocol; we're looking for an +OK
     */

    case NETSEC_SASL_FINISH:
	line = netsec_readline(nsc, &len, errstr);
	if (line == NULL)
	    return NOTOK;

	if (!has_prefix(line, "+OK")) {
	    netsec_err(errstr, "Authentication failed: %s", line);
	    return NOTOK;
	}
	break;

    /*
     * Cancel the SASL exchange in the middle of the commands; for
     * POP, that's a single "*".
     *
     * It's unclear to me if I should be returning errors up; I finally
     * decided the answer should be "yes", and if the upper layer wants to
     * ignore them that's their choice.
     */

    case NETSEC_SASL_CANCEL:
	rc = netsec_printf(nsc, errstr, "*\r\n");
	if (rc == OK)
	    rc = netsec_flush(nsc, errstr);
	if (rc != OK)
	    return NOTOK;
	break;
    }

    return OK;
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
pop_retr (int msgno, int (*action)(void *, char *), void *closure)
{
    return traverse (action, closure, "RETR %d", msgno);
}


static int
traverse (int (*action)(void *, char *), void *closure, const char *fmt, ...)
{
    int result, snoopstate;
    va_list ap;
    char buffer[sizeof(response)];

    va_start(ap, fmt);
    result = vcommand (fmt, ap);
    va_end(ap);

    if (result == NOTOK)
	return NOTOK;
    strncpy (buffer, response, sizeof(buffer));

    if ((snoopstate = netsec_get_snoop(nsc)))
	netsec_set_snoop(nsc, 0);

    for (;;) {
        result = multiline();
        if (result == OK) {
            result = (*action)(closure, response);
            if (result == OK)
                continue;
        } else if (result == DONE) {
            strncpy(response, buffer, sizeof(response));
            result = OK;
        }
        break;
    }

    netsec_set_snoop(nsc, snoopstate);
    return result;
}


int
pop_dele (int msgno)
{
    return command ("DELE %d", msgno);
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
	netsec_shutdown(nsc);

    return OK;
}


static int
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
    /* char *cp; */
    char *errstr;

    if (netsec_vprintf(nsc, &errstr, fmt, ap) != OK) {
	strncpy(response, errstr, sizeof(response));
	response[sizeof(response) - 1] = '\0';
	free(errstr);
	return NOTOK;
    }

    if (netsec_printf(nsc, &errstr, "\r\n") != OK) {
	strncpy(response, errstr, sizeof(response));
	response[sizeof(response) - 1] = '\0';
	free(errstr);
	return NOTOK;
    }

    if (netsec_flush(nsc, &errstr) != OK) {
	strncpy(response, errstr, sizeof(response));
	response[sizeof(response) - 1] = '\0';
	free(errstr);
	return NOTOK;
    }

    switch (pop_getline (response, sizeof response, nsc)) {
	case OK: 
	    if (poprint)
		fprintf (stderr, "<--- %s\n", response);
	    return *response == '+' ? OK : NOTOK;

	case NOTOK: 
	case DONE: 
	    if (poprint) {
		fputs(response, stderr);
                putc('\n', stderr);
            }
	    return NOTOK;
    }

    return NOTOK;	/* NOTREACHED */
}


static int
multiline (void)
{
    char buffer[BUFSIZ + LEN(TRM)];

    if (pop_getline (buffer, sizeof buffer, nsc) != OK)
	return NOTOK;
    if (has_prefix(buffer, TRM)) {
	if (buffer[LEN(TRM)] == 0)
	    return DONE;
        strncpy (response, buffer + LEN(TRM), sizeof(response));
    }
    else
	strncpy (response, buffer, sizeof(response));

    return OK;
}

/*
 * This is now just a thin wrapper around netsec_readline().
 */

static int
pop_getline (char *s, int n, netsec_context *ns)
{
    /* int c = -2; */
    char *p;
    size_t len, destlen;
    /* int rc; */
    char *errstr;

    p = netsec_readline(ns, &len, &errstr);

    if (p == NULL) {
	strncpy(response, errstr, sizeof(response));
	response[sizeof(response) - 1] = '\0';
	free(errstr);
	return NOTOK;
    }

    /*
     * If we had an error, it should have been returned already.  Since
     * netsec_readline() strips off the CR-LF ending, just copy the existing
     * buffer into response now.
     *
     * We get a length back from netsec_readline, but the rest of the POP
     * code doesn't handle it; the assumptions are that everything from
     * the network can be represented as C strings.  That should get fixed
     * someday.
     */

    destlen = min(len, (size_t)(n - 1));

    memcpy(s, p, destlen);
    s[destlen] = '\0';
    
    return OK;
}
