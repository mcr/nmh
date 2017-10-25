/* imaptest.c -- A program to send commands to an IMAP server 
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <h/netsec.h>
#include <stdarg.h>
#include "h/done.h"
#include "sbr/base64.h"

#define IMAPTEST_SWITCHES \
    X("host hostname", 0, HOSTSW) \
    X("user username", 0, USERSW) \
    X("port name/number", 0, PORTSW) \
    X("snoop", 0, SNOOPSW) \
    X("nosnoop", 0, NOSNOOPSW) \
    X("sasl", 0, SASLSW) \
    X("nosasl", 0, NOSASLSW) \
    X("saslmech", 0, SASLMECHSW) \
    X("authservice", 0, AUTHSERVICESW) \
    X("tls", 0, TLSSW) \
    X("notls", 0, NOTLSSW) \
    X("initialtls", 0, INITIALTLSSW) \
    X("append filename", 0, APPENDSW) \
    X("timestamp", 0, TIMESTAMPSW) \
    X("notimestamp", 0, NOTIMESTAMPSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(IMAPTEST);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(IMAPTEST, switches);
#undef X

struct imap_cmd;

struct imap_cmd {
    char tag[16];		/* Command tag */
    struct imap_cmd *next;	/* Next pointer */
};

static struct imap_cmd *cmdqueue = NULL;
static char *saslmechs = NULL;
svector_t imap_capabilities = NULL;

static int imap_sasl_callback(enum sasl_message_type, unsigned const char *,
			      unsigned int, unsigned char **, unsigned int *,
			      void *, char **);

static void parse_capability(const char *, unsigned int len);
static int capability_set(const char *);
static void clear_capability(void);
static int have_capability(void);
static int send_imap_command(netsec_context *, int noflush, char **errstr,
			     const char *fmt, ...);
static int get_imap_response(netsec_context *, const char *token,
			     char **tokenresp, char **status, int failerr,
			     char **errstr);

int
main (int argc, char **argv)
{
    bool timestamp = false, sasl = false, tls = false, initialtls = false;
    bool snoop = false;
    int fd;
    char *saslmech = NULL, *host = NULL, *port = "143", *user = NULL;
    char *cp, **argp, buf[BUFSIZ], *oauth_svc = NULL, *errstr, **arguments, *p;
    netsec_context *nsc = NULL;
    size_t len;

    if (nmh_init(argv[0], 1)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
	    case AMBIGSW:
		ambigsw (cp, switches);
		done (1);
	    case UNKWNSW:
		adios (NULL, "-%s unknown", cp);

	    case HELPSW:
		snprintf (buf, sizeof(buf), "%s [switches] +folder command "
			  "[command ...]", invo_name);
		print_help (buf, switches, 1);
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case HOSTSW:
		if (!(host = *argp++) || *host == '-')
		    adios(NULL, "missing argument to %s", argp[-2]);
		continue;
	    case PORTSW:
		if (!(port = *argp++) || *port == '-')
		    adios(NULL, "missing argument to %s", argp[-2]);
		continue;
	    case USERSW:
		if (!(user = *argp++) || *user == '-')
		    adios(NULL, "missing argument to %s", argp[-2]);
		continue;

	    case SNOOPSW:
		snoop = true;
		continue;
	    case NOSNOOPSW:
	    	snoop = false;
		continue;
	    case SASLSW:
		sasl = true;
		continue;
	    case NOSASLSW:
		sasl = false;
		continue;
	    case SASLMECHSW:
		if (!(saslmech = *argp++) || *saslmech == '-')
		    adios(NULL, "missing argument to %s", argp[-2]);
		continue;
	    case AUTHSERVICESW:
	    	if (!(oauth_svc = *argp++) || *oauth_svc == '-')
		    adios(NULL, "missing argument to %s", argp[-2]);
		continue;
	    case TLSSW:
		tls = true;
		initialtls = false;
		continue;
	    case INITIALTLSSW:
		tls = false;
		initialtls = true;
		continue;
	    case NOTLSSW:
		tls = false;
		initialtls = false;
		continue;
	    case TIMESTAMPSW:
		timestamp = true;
		continue;
	    case NOTIMESTAMPSW:
		timestamp = false;
		continue;
	    }
	}
    }

    if (! host)
	adios(NULL, "A hostname must be given with -host");

    nsc = netsec_init();

    if (user)
	netsec_set_userid(nsc, user);

    netsec_set_hostname(nsc, host);

    if (snoop)
	netsec_set_snoop(nsc, 1);

    if (oauth_svc) {
	if (netsec_set_oauth_service(nsc, oauth_svc) != OK) {
	    adios(NULL, "OAuth2 not supported");
	}
    }

    if ((fd = client(host, port, buf, sizeof(buf), snoop)) == NOTOK)
	adios(NULL, "Connect failed: %s", buf);

    netsec_set_fd(nsc, fd, fd);
    netsec_set_snoop(nsc, snoop);

    if (initialtls || tls) {
	if (netsec_set_tls(nsc, 1, 0, &errstr) != OK)
	    adios(NULL, errstr);

	if (initialtls && netsec_negotiate_tls(nsc, &errstr) != OK)
	    adios(NULL, errstr);
    }

    if (sasl) {
	if (netsec_set_sasl_params(nsc, "imap", saslmech, imap_sasl_callback,
				   nsc, &errstr) != OK)
	    adios(NULL, errstr);
    }

    if ((cp = netsec_readline(nsc, &len, &errstr)) == NULL) {
	adios(NULL, errstr);
    }

    if (has_prefix(cp, "* BYE")) {
	fprintf(stderr, "Connection rejected: %s\n", cp + 5);
	goto finish;
    }

    if (!has_prefix(cp, "* OK") && !has_prefix(cp, "* PREAUTH")) {
	fprintf(stderr, "Invalid server response: %s\n", cp);
	goto finish;
    }

    if ((p = strchr(cp + 2, ' ')) && *(p + 1) != '\0' &&
	has_prefix(p + 1, "[CAPABILITY ")) {
	/*
	 * Parse the capability list out to the end
	 */
	char *q;
	p += 13;	/* 1 + [CAPABILITY + space */

	if (!(q = strchr(p, ']'))) {
	    fprintf(stderr, "Cannot find end of CAPABILITY announcement\n");
	    goto finish;
	}

	parse_capability(p, q - p);
    } else {
        char *capstring;

	if (send_imap_command(nsc, 0, &errstr, "CAPABILITY") != OK) {
	    fprintf(stderr, "Unable to send CAPABILITY command: %s\n", errstr);
	    goto finish;
	}

	if (get_imap_response(nsc, "CAPABILITY", &capstring, NULL, 1,
			      &errstr) != OK) {
	    fprintf(stderr, "Cannot get CAPABILITY response: %s\n", errstr);
	    goto finish;
	}

	if (! capstring) {
	    fprintf(stderr, "No CAPABILITY response seen\n");
	    goto finish;
	}

	parse_capability(capstring, strlen(capstring));
	free(capstring);
    }

    if (tls) {
	if (!capability_set("STARTTLS")) {
	    fprintf(stderr, "Requested STARTTLS with -tls, but IMAP server "
		    "has no support for STARTTLS\n");
	    goto finish;
	}
	if (send_imap_command(nsc, 0, &errstr, "STARTTLS") != OK) {
	    fprintf(stderr, "Unable to issue STARTTLS: %s\n", errstr);
	    goto finish;
	}

	if (get_imap_response(nsc, NULL, NULL, NULL, 1, &errstr) != OK) {
	    fprintf(stderr, "STARTTLS failed: %s\n", errstr);
	    goto finish;
	}
	if (netsec_negotiate_tls(nsc, &errstr) != OK) {
	    adios(NULL, errstr);
	}
    }

    if (sasl) {
	if (netsec_negotiate_sasl(nsc, saslmechs, &errstr) != OK) {
	    fprintf(stderr, "SASL negotiation failed: %s\n", errstr);
	    goto finish;
	}
    } else {
	if (capability_set("LOGINDISABLED")) {
	    fprintf(stderr, "User did not request SASL, but LOGIN "
		    "is disabled\n");
	    goto finish;
	}
    }

    send_imap_command(nsc, 0, NULL, "LOGOUT");
    get_imap_response(nsc, NULL, NULL, NULL, 0, NULL);

finish:
    netsec_shutdown(nsc);

    exit(0);
}

/*
 * Parse a capability string
 *
 * Put these into an svector so we can check for stuff later.  But special
 * things, like AUTH, we parse now
 */

static void
parse_capability(const char *cap, unsigned int len)
{
    char *str = mh_xmalloc(len + 1);
    char **caplist;
    int i;

    trunccpy(str, cap, len + 1);
    caplist = brkstring(str, " ", NULL);

    if (imap_capabilities) {
	svector_free(imap_capabilities);
    }

    imap_capabilities = svector_create(32);

    if (saslmechs) {
	free(saslmechs);
	saslmechs = NULL;
    }

    for (i = 0; caplist[i] != NULL; i++) {
	if (has_prefix(caplist[i], "AUTH=") && caplist[i] + 5 != '\0') {
	    if (saslmechs) {
		saslmechs = add(" ", saslmechs);
	    }
	    saslmechs = add(caplist[i] + 5, saslmechs);
	} else {
	    svector_push_back(imap_capabilities, getcpy(caplist[i]));
	}
    }

    free(caplist);
    free(str);
}

/*
 * Return 1 if a particular capability is set
 */

static int
capability_set(const char *capability)
{
    if (!imap_capabilities)
	return 0;

    return svector_find(imap_capabilities, capability) != NULL;
}

/*
 * Clear the CAPABILITY list (used after we call STARTTLS, for example)
 */

static void
capability_clear(void)
{
    svector_free(imap_capabilities);
    imap_capabilities = NULL;
}

static int
have_capability(void)
{
    return imap_capabilities != NULL;
}

/*
 * Our SASL callback, which handles the SASL authentication dialog
 */

static int
imap_sasl_callback(enum sasl_message_type mtype, unsigned const char *indata,
		   unsigned int indatalen, unsigned char **outdata,
		   unsigned int *outdatalen, void *context, char **errstr)
{
    int rc, snoopoffset;
    char *mech, *line;
    size_t len;
    netsec_context *nsc = (netsec_context *) context;

    switch (mtype) {
    case NETSEC_SASL_START:
	/*
	 * Generate our AUTHENTICATE message.
	 *
	 * If SASL-IR capability is set, we can include any initial response
	 * in the AUTHENTICATE command.  Otherwise we have to wait for
	 * the first server response (which should be blank).
	 */

	mech = netsec_get_sasl_mechanism(nsc);

	if (indatalen) {
	    char *b64data;
	    b64data = mh_xmalloc(BASE64SIZE(indatalen));
	    writeBase64raw(indata, indatalen, (unsigned char *) b64data);
	    if (capability_set("SASL-IR")) {
		netsec_set_snoop_callback(nsc, netsec_b64_snoop_decoder,
					  &snoopoffset);
		/*
		 * Sigh. We're assuming a short tag length here.  Really,
		 * I guess we should figure out a way to get the length
		 * of the next tag.
		 */
		snoopoffset = 17 + strlen(mech);
		rc = send_imap_command(nsc, 0, errstr, "AUTHENTICATE %s %s",
				       mech, b64data);
		free(b64data);
		netsec_set_snoop_callback(nsc, NULL, NULL);
		if (rc)
		    return NOTOK;
	    } else {
		rc = send_imap_command(nsc, 0, errstr, "AUTHENTICATE %s", mech);
		line = netsec_readline(nsc, &len, errstr);
		if (! line)
		    return NOTOK;
		/*
		 * We should get a "+ ", nothing else.
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
	    }
	} else {
	    if (send_imap_command(nsc, 0, errstr, "AUTHENTICATE %s",
				  mech) != OK)
		return NOTOK;
	}

	break;

    /*
     * Get a response, decode it and process it.
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
     * Simple request encoding
     */

    case NETSEC_SASL_WRITE:
	if (indatalen > 0) {
	    unsigned char *b64data;
	    b64data = mh_xmalloc(BASE64SIZE(indatalen));
	    writeBase64raw(indata, indatalen, b64data);
	    rc = netsec_printf(nsc, errstr, "%s", b64data);
	    free(b64data);
	    if (rc != OK)
		return NOTOK;
	}

	if (netsec_printf(nsc, errstr, "\r\n") != OK)
	    return NOTOK;

	netsec_set_snoop_callback(nsc, netsec_b64_snoop_decoder, NULL);
	rc = netsec_flush(nsc, errstr);
	netsec_set_snoop_callback(nsc, NULL, NULL);
	if (rc != OK)
	    return NOTOK;
	break;

    /*
     * Finish protocol
     */

    case NETSEC_SASL_FINISH:
    	if (get_imap_response(nsc, NULL, NULL, &line, 1, errstr) != OK)
	    return NOTOK;
	/*
	 * We MIGHT get a capability response here.  If so, be sure we
	 * parse it.
	 */

	if (has_prefix(line, "OK [CAPABILITY ")) {
	    char *p = line + 15, *q;
	    q = strchr(p, ']');

	    if (q)
		parse_capability(p, q - p);
	}

	break;

    /*
     * Cancel an authentication dialog
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
 * Send a single command to the IMAP server.
 */

static int
send_imap_command(netsec_context *nsc, int noflush, char **errstr,
		  const char *fmt, ...)
{
    static unsigned int seq = 0;	/* Tag sequence number */
    va_list ap;
    int rc;
    struct imap_cmd *cmd;

    cmd = mh_xmalloc(sizeof(*cmd));

    snprintf(cmd->tag, sizeof(cmd->tag), "A%u ", seq++);

    if (netsec_write(nsc, cmd->tag, strlen(cmd->tag), errstr) != OK) {
	free(cmd);
	return NOTOK;
    }

    va_start(ap, fmt);
    rc = netsec_vprintf(nsc, errstr, fmt, ap);
    va_end(ap);

    if (rc == OK)
	rc = netsec_write(nsc, "\r\n", 2, errstr);

    if (rc != OK) {
	free(cmd);
	return NOTOK;
    }

    if (!noflush && netsec_flush(nsc, errstr) != OK) {
	free(cmd);
	return NOTOK;
    }

    cmd->next = cmdqueue;
    cmdqueue = cmd;

    return OK;
}

/*
 * Get all outstanding responses.  If we were passed in a token string
 * to look for, return it.
 */

static int
get_imap_response(netsec_context *nsc, const char *token, char **tokenresponse,
		  char **status, int failerr, char **errstr)
{
    char *line;
    struct imap_cmd *cmd;
    int numerrs = 0;

    if (tokenresponse)
	*tokenresponse = NULL;

getline:
    while (cmdqueue) {
	if (!(line = netsec_readline(nsc, NULL, errstr)))
	    return NOTOK;
	if (has_prefix(line, "* ") && *(line + 2) != '\0') {
	    if (token && tokenresponse && has_prefix(line + 2, token)) {
		if (*tokenresponse)
		    free(*tokenresponse);
		*tokenresponse = getcpy(line + 2);
	    }
	} else {

	    if (has_prefix(line, cmdqueue->tag)) {
		cmd = cmdqueue;
		cmdqueue = cmd->next;
		free(cmd);
	    } else {
		for (cmd = cmdqueue; cmd->next != NULL; cmd = cmd->next) {
		    if (has_prefix(line, cmd->next->tag)) {
		        struct imap_cmd *cmd2 = cmd->next;
			cmd->next = cmd->next->next;
			if (failerr && strncmp(line + strlen(cmd2->tag),
							"OK ", 3) != 0) {
			    numerrs++;
			    netsec_err(errstr, "%s", line + strlen(cmd2->tag));
			}
			free(cmd2);
			if (status)
			    *status = getcpy(line);
			goto getline;
		    }
		    netsec_err(errstr, "Non-matching response line: %s\n",
			       line);
		    return NOTOK;
		}
	    }
	}
    }

    return numerrs == 0 ? OK : NOTOK;
}
