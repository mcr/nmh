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
#include <sys/time.h>
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
    X("queue", 0, QUEUESW) \
    X("noqueue", 0, NOQUEUESW) \
    X("append filename", 0, APPENDSW) \
    X("afolder foldername", 0, AFOLDERSW) \
    X("batch filename", 0, BATCHSW) \
    X("timestamp", 0, TIMESTAMPSW) \
    X("notimestamp", 0, NOTIMESTAMPSW) \
    X("timeout", 0, TIMEOUTSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(IMAPTEST);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(IMAPTEST, switches);
#undef X

struct imap_msg;

struct imap_msg {
    char *command;		/* Command to send */
    char *folder;		/* Folder (for append) */
    bool queue;			/* If true, queue for later delivery */
    bool append;		/* If true, append "command" to mbox */
    size_t msgsize;		/* RFC822 size of message */
    struct imap_msg *next;	/* Next pointer */
};

struct imap_msg *msgqueue_head = NULL;
struct imap_msg *msgqueue_tail = NULL;

struct imap_cmd;

struct imap_cmd {
    char tag[16];		/* Command tag */
    struct timeval start;	/* Time command was sent */
    char prefix[64];		/* Command prefix */
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
static int send_imap_command(netsec_context *, bool noflush, char **errstr,
			     const char *fmt, ...) CHECK_PRINTF(4, 5);
static int send_append(netsec_context *, struct imap_msg *, char **errstr);
static int get_imap_response(netsec_context *, const char *token,
			     char **tokenresp, char **status, bool contok,
			     bool failerr, char **errstr);

static void ts_report(struct timeval *tv, const char *fmt, ...)
		      CHECK_PRINTF(2, 3);
static void imap_negotiate_tls(netsec_context *);

static void add_msg(bool queue, struct imap_msg **, const char *fmt, ...)
		    CHECK_PRINTF(3, 4);
static void add_append(const char *filename, const char *folder, bool queue);

static void batchfile(const char *filename, char *afolder, bool queue);

static size_t rfc822size(const char *filename);

static bool timestamp = false;

int
main (int argc, char **argv)
{
    bool sasl = false, tls = false, initialtls = false;
    bool snoop = false, queue = false;
    int fd, timeout = 0;
    char *saslmech = NULL, *host = NULL, *port = "143", *user = NULL;
    char *cp, **argp, buf[BUFSIZ], *oauth_svc = NULL, *errstr, **arguments, *p;
    char *afolder = NULL;
    netsec_context *nsc = NULL;
    struct imap_msg *imsg;
    size_t len;
    struct timeval tv_start, tv_connect, tv_auth;

    if (nmh_init(argv[0], true, true)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
	    case AMBIGSW:
		ambigsw (cp, switches);
		done (1);
	    case UNKWNSW:
		die("-%s unknown", cp);

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
		    die("missing argument to %s", argp[-2]);
		continue;
	    case PORTSW:
		if (!(port = *argp++) || *port == '-')
		    die("missing argument to %s", argp[-2]);
		continue;
	    case USERSW:
		if (!(user = *argp++) || *user == '-')
		    die("missing argument to %s", argp[-2]);
		continue;

	    case AFOLDERSW:
		if (!(afolder = *argp++) || *afolder == '-')
		    die("missing argument to %s", argp[-2]);
		continue;
	    case APPENDSW:
		if (!*argp || (**argp == '-'))
		    die("missing argument to %s", argp[-1]);
		if (! afolder)
		    die("Append folder must be set with -afolder first");
		add_append(*argp++, afolder, queue);
		continue;

	    case BATCHSW:
		if (! *argp || (**argp == '-'))
		    die("missing argument to %s", argp[-1]);
		batchfile(*argp++, afolder, queue);
		continue;

	    case TIMEOUTSW:
		if (! *argp || (**argp == '-'))
		    die("missing argument to %s", argp[-1]);
		if (! (timeout = atoi(*argp++)))
		    die("Invalid timeout: %s", argp[-1]);
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
		    die("missing argument to %s", argp[-2]);
		continue;
	    case AUTHSERVICESW:
	    	if (!(oauth_svc = *argp++) || *oauth_svc == '-')
		    die("missing argument to %s", argp[-2]);
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
	    case QUEUESW:
		queue = true;
		continue;
	    case NOQUEUESW:
		queue = false;
		continue;
	    case TIMESTAMPSW:
		timestamp = true;
		continue;
	    case NOTIMESTAMPSW:
		timestamp = false;
		continue;
	    }
	} else if (*cp == '+') {
	    if (*(cp + 1) == '\0')
		die("Invalid null folder name");
	    add_msg(false, NULL, "SELECT \"%s\"", cp + 1);
	} else {
	    add_msg(queue, NULL, "%s", cp);
	}
    }

    if (! host)
	die("A hostname must be given with -host");

    nsc = netsec_init();

    if (user)
	netsec_set_userid(nsc, user);

    netsec_set_hostname(nsc, host);

    if (timeout)
	netsec_set_timeout(nsc, timeout);

    if (snoop)
	netsec_set_snoop(nsc, 1);

    if (oauth_svc) {
	if (netsec_set_oauth_service(nsc, oauth_svc) != OK) {
	    die("OAuth2 not supported");
	}
    }

    if (timestamp)
	gettimeofday(&tv_start, NULL);

    if ((fd = client(host, port, buf, sizeof(buf), snoop)) == NOTOK)
	die("Connect failed: %s", buf);

    if (timestamp) {
	ts_report(&tv_start, "Connect time");
	gettimeofday(&tv_connect, NULL);
    }

    netsec_set_fd(nsc, fd, fd);
    netsec_set_snoop(nsc, snoop);

    if (initialtls || tls) {
	if (netsec_set_tls(nsc, 1, 0, &errstr) != OK)
	    die("%s", errstr);

	if (initialtls)
	    imap_negotiate_tls(nsc);
    }

    if (sasl) {
	if (netsec_set_sasl_params(nsc, "imap", saslmech, imap_sasl_callback,
				   nsc, &errstr) != OK)
	    die("%s", errstr);
    }

    if ((cp = netsec_readline(nsc, &len, &errstr)) == NULL) {
	die("%s", errstr);
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

	if (send_imap_command(nsc, false, &errstr, "CAPABILITY") != OK) {
	    fprintf(stderr, "Unable to send CAPABILITY command: %s\n", errstr);
	    goto finish;
	}

	if (get_imap_response(nsc, "CAPABILITY ", &capstring, NULL, false, true,
			      &errstr) != OK) {
	    fprintf(stderr, "Cannot get CAPABILITY response: %s\n", errstr);
	    goto finish;
	}

	if (! capstring) {
	    fprintf(stderr, "No CAPABILITY response seen\n");
	    goto finish;
	}

	p = capstring + 11;	/* "CAPABILITY " */

	parse_capability(p, strlen(p));
	free(capstring);
    }

    if (tls) {
	if (!capability_set("STARTTLS")) {
	    fprintf(stderr, "Requested STARTTLS with -tls, but IMAP server "
		    "has no support for STARTTLS\n");
	    goto finish;
	}
	if (send_imap_command(nsc, false, &errstr, "STARTTLS") != OK) {
	    fprintf(stderr, "Unable to issue STARTTLS: %s\n", errstr);
	    goto finish;
	}

	if (get_imap_response(nsc, NULL, NULL, NULL, false, true,
			      &errstr) != OK) {
	    fprintf(stderr, "STARTTLS failed: %s\n", errstr);
	    goto finish;
	}

	imap_negotiate_tls(nsc);
    }

    if (sasl) {
	if (netsec_negotiate_sasl(nsc, saslmechs, &errstr) != OK) {
	    fprintf(stderr, "SASL negotiation failed: %s\n", errstr);
	    goto finish;
	}
	/*
	 * Sigh. If we negotiated a SSF AND we got a capability response
	 * as part of the AUTHENTICATE OK message, we can't actually trust
	 * it because it's not protected at that point.  So discard the
	 * capability list and we will generate a fresh CAPABILITY command
	 * later.
	 */
	if (netsec_get_sasl_ssf(nsc) > 0) {
	    clear_capability();
	}
    } else {
	if (capability_set("LOGINDISABLED")) {
	    fprintf(stderr, "User did not request SASL, but LOGIN "
		    "is disabled\n");
	    goto finish;
	}
    }

    if (!have_capability()) {
        char *capstring;

	if (send_imap_command(nsc, false, &errstr, "CAPABILITY") != OK) {
	    fprintf(stderr, "Unable to send CAPABILITY command: %s\n", errstr);
	    goto finish;
	}

	if (get_imap_response(nsc, "CAPABILITY ", &capstring, NULL, false,
			      true, &errstr) != OK) {
	    fprintf(stderr, "Cannot get CAPABILITY response: %s\n", errstr);
	    goto finish;
	}

	if (! capstring) {
	    fprintf(stderr, "No CAPABILITY response seen\n");
	    goto finish;
	}

	p = capstring + 11;	/* "CAPABILITY " */

	parse_capability(p, strlen(p));
	free(capstring);
    }

    if (timestamp) {
	ts_report(&tv_connect, "Authentication time");
	gettimeofday(&tv_auth, NULL);
    }

    while (msgqueue_head != NULL) {
	imsg = msgqueue_head;

	if (imsg->append) {
	    if (send_append(nsc, imsg, &errstr) != OK) {
		fprintf(stderr, "Cannot send APPEND for %s to mbox %s: %s\n",
			imsg->command, imsg->folder, errstr);
		free(errstr);
		goto finish;
	    }
	} else if (send_imap_command(nsc, imsg->queue, &errstr, "%s",
				     imsg->command) != OK) {
		fprintf(stderr, "Cannot send command \"%s\": %s\n",
			imsg->command, errstr);
		free(errstr);
		goto finish;
	    }

	if (! imsg->queue) {
	    if (get_imap_response(nsc, NULL, NULL, NULL, false, false,
				  &errstr) != OK) {
		fprintf(stderr, "Unable to get response for command "
			"\"%s\": %s\n", imsg->command, errstr);
		goto finish;
	    }
	}

	msgqueue_head = imsg->next;

	free(imsg->command);
	free(imsg->folder);
	free(imsg);
    }

    /*
     * Flush out any pending network data and get any responses
     */

    if (netsec_flush(nsc, &errstr) != OK) {
	fprintf(stderr, "Error performing final network flush: %s\n", errstr);
	free(errstr);
    }

    if (get_imap_response(nsc, NULL, NULL, NULL, false, false, &errstr) != OK) {
	fprintf(stderr, "Error fetching final command responses: %s\n", errstr);
	free(errstr);
    }

    if (timestamp)
	ts_report(&tv_auth, "Total command execution time");

    send_imap_command(nsc, false, NULL, "LOGOUT");
    get_imap_response(nsc, NULL, NULL, NULL, false, false, NULL);

finish:
    netsec_shutdown(nsc);

    if (timestamp)
	ts_report(&tv_start, "Total elapsed time");

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
	if (has_prefix(caplist[i], "AUTH=") && *(caplist[i] + 5) != '\0') {
	    if (saslmechs) {
		saslmechs = add(" ", saslmechs);
	    }
	    saslmechs = add(caplist[i] + 5, saslmechs);
	} else {
	    svector_push_back(imap_capabilities, getcpy(caplist[i]));
	}
    }

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
clear_capability(void)
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
    char *mech, *line, *capstring, *p;
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
        line = NULL;
	if (get_imap_response(nsc, "CAPABILITY ", &capstring, &line,
			      false, true, errstr) != OK)
	    return NOTOK;
	/*
	 * We MIGHT get a capability response here.  If so, be sure we
	 * parse it.  We ALSO might get a untagged CAPABILITY response.
	 * Which one should we prefer?  I guess we'll go with the untagged
	 * one.
	 */

	if (capstring) {
	    p = capstring + 11;		/* "CAPABILITY " */
	    parse_capability(p, strlen(p));
	    free(capstring);
	} else if (line && has_prefix(line, "OK [CAPABILITY ")) {
	    char *q;

	    p = line + 15;
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
send_imap_command(netsec_context *nsc, bool noflush, char **errstr,
		  const char *fmt, ...)
{
    static unsigned int seq = 0;	/* Tag sequence number */
    va_list ap;
    int rc;
    struct imap_cmd *cmd;

    cmd = mh_xmalloc(sizeof(*cmd));

    snprintf(cmd->tag, sizeof(cmd->tag), "A%u ", seq++);

    if (timestamp) {
	char *p;
	va_start(ap, fmt);
	vsnprintf(cmd->prefix, sizeof(cmd->prefix), fmt, ap);
	va_end(ap);

    	p = strchr(cmd->prefix, ' ');

	if (p)
	    *p = '\0';

	gettimeofday(&cmd->start, NULL);
    }

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
 * Send an APPEND to the server, which requires some extra semantics
 */

static int
send_append(netsec_context *nsc, struct imap_msg *imsg, char **errstr)
{
    FILE *f;
    bool nonsynlit = false;
    char *status = NULL, *line = NULL;
    size_t linesize = 0;
    ssize_t rc;

    /*
     * If we have the LITERAL+ extension, or we have LITERAL- and our
     * message is 4096 bytes or less, we can do it all in one message.
     * Otherwise we have to wait for a contination marker (+).
     */

    if (capability_set("LITERAL+") ||
		(capability_set("LITERAL-") && imsg->msgsize <= 4096)) {
	nonsynlit = true;
    }

    /*
     * Send our APPEND command
     */

    if (send_imap_command(nsc, nonsynlit, errstr, "APPEND \"%s\" {%u%s}",
			  imsg->folder, (unsigned int) imsg->msgsize,
			  nonsynlit ? "+" : "") != OK)
	return NOTOK;

    /*
     * If we need to wait for a syncing literal, do that now
     */

    if (! nonsynlit) {
	if (get_imap_response(nsc, NULL, NULL, &status, true,
			      true, errstr) != OK) {
	    imsg->queue = true;	/* XXX Sigh */
	    fprintf(stderr, "APPEND command failed: %s\n", *errstr);
	    free(*errstr);
	    return OK;
	}
	if (!(status && has_prefix(status, "+"))) {
	    netsec_err(errstr, "Expected contination (+), but got: %s", status);
	    free(status);
	    return NOTOK;
	}
	free(status);
    }

    /*
     * Now write the message out, but make sure we end each line with \r\n
     */

    if ((f = fopen(imsg->command, "r")) == NULL) {
	netsec_err(errstr, "Unable to open %s: %s", imsg->command,
		   strerror(errno));
	return NOTOK;
    }

    while ((rc = getline(&line, &linesize, f)) > 0) {
	if (rc > 1 && line[rc - 1] == '\n' && line[rc - 2] == '\r') {
	    if (netsec_write(nsc, line, rc, errstr) != OK) {
		free(line);
		fclose(f);
		return NOTOK;
	    }
	} else {
	    if (line[rc - 1] == '\n')
		rc--;
	    if (netsec_write(nsc, line, rc, errstr) != OK) {
		free(line);
		fclose(f);
		return NOTOK;
	    }
	    if (netsec_write(nsc, "\r\n", 2, errstr) != OK) {
		free(line);
		fclose(f);
		return NOTOK;
	    }
	}
    }

    free(line);

    if (! feof(f)) {
	netsec_err(errstr, "Error reading %s: %s", imsg->command,
		   strerror(errno));
	fclose(f);
	return NOTOK;
    }

    fclose(f);

    /*
     * Send a final \r\n for the end of the command
     */

    if (netsec_write(nsc, "\r\n", 2, errstr) != OK)
	return NOTOK;

    if (! imsg->queue)
	return netsec_flush(nsc, errstr);

    return OK;
}

/*
 * Get all outstanding responses.  If we were passed in a token string
 * to look for, return it.
 */

static int
get_imap_response(netsec_context *nsc, const char *token, char **tokenresponse,
		  char **status, bool condok, bool failerr, char **errstr)
{
    char *line;
    struct imap_cmd *cmd;
    bool numerrs = false;

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
	} if (condok && has_prefix(line, "+")) {
	    if (status) {
		*status = getcpy(line);
	    }
	    /*
	     * Special case; return now but don't dequeue the tag,
	     * since we will want to get final result later.
	     */
	    return OK;
	} else {

	    if (has_prefix(line, cmdqueue->tag)) {
		cmd = cmdqueue;
		if (timestamp)
		    ts_report(&cmd->start, "Command (%s) execution time",
			      cmd->prefix);
		cmdqueue = cmd->next;
		free(cmd);
	    } else {
		for (cmd = cmdqueue; cmd->next != NULL; cmd = cmd->next) {
		    if (has_prefix(line, cmd->next->tag)) {
		        struct imap_cmd *cmd2 = cmd->next;
			cmd->next = cmd->next->next;
			if (failerr && strncmp(line + strlen(cmd2->tag),
							"OK ", 3) != 0) {
			    numerrs = true;
			    netsec_err(errstr, "%s", line + strlen(cmd2->tag));
			}
			if (timestamp)
			    ts_report(&cmd2->start, "Command (%s) execution "
				      "time", cmd2->prefix);
			free(cmd2);
			if (status)
			    *status = getcpy(line);
			goto getline;
		    }
		}
	    }
	}
    }

    return numerrs ? NOTOK : OK;
}

/*
 * Add an IMAP command to the msg queue
 */

static void
add_msg(bool queue, struct imap_msg **ret_imsg, const char *fmt, ...)
{
    struct imap_msg *imsg;
    va_list ap;
    size_t msgbufsize;
    char *msg = NULL;
    int rc = 63;

    do {
	msgbufsize = rc + 1;
	msg = mh_xrealloc(msg, msgbufsize);
	va_start(ap, fmt);
	rc = vsnprintf(msg, msgbufsize, fmt, ap);
	va_end(ap);
    } while (rc >= (int) msgbufsize);

    imsg = mh_xmalloc(sizeof(*imsg));

    imsg->command = msg;
    imsg->folder = NULL;
    imsg->append = false;
    imsg->queue = queue;
    imsg->next = NULL;

    if (msgqueue_head == NULL) {
	msgqueue_head = imsg;
	msgqueue_tail = imsg;
    } else {
	msgqueue_tail->next = imsg;
	msgqueue_tail = imsg;
    }

    if (ret_imsg)
	*ret_imsg = imsg;
}

/*
 * Add an APPEND command to the queue
 */

static void
add_append(const char *filename, const char *folder, bool queue)
{
    size_t filesize = rfc822size(filename);
    struct imap_msg *imsg;

    add_msg(queue, &imsg, "%s", filename);

    imsg->folder = getcpy(folder);
    imsg->append = true;
    imsg->msgsize = filesize;
}

/*
 * Process a batch file, which can contain commands (and some arguments)
 */

static void
batchfile(const char *filename, char *afolder, bool queue)
{
    FILE *f;
    char *line = NULL;
    size_t linesize = 0;
    ssize_t rc;
    bool afolder_alloc = false;

    if (!(f = fopen(filename, "r"))) {
	die("Unable to open batch file %s: %s", filename, strerror(errno));
    }

    while ((rc = getline(&line, &linesize, f)) > 0) {
	line[rc - 1] = '\0';
	if (*line == '-') {
	    switch (smatch (line + 1, switches)) {
	    case QUEUESW:
		queue = true;
		continue;
	    case NOQUEUESW:
		queue = false;
		continue;
	    case AFOLDERSW:
		if (afolder_alloc)
		    free(afolder);
		rc = getline(&line, &linesize, f);
		if (rc <= 0)
		    die("Unable to read next line for -afolder");
		if (rc == 1)
		    die("Folder name cannot be blank");
		line[rc - 1] = '\0';
		afolder = getcpy(line);
		afolder_alloc = true;
		continue;

	    case APPENDSW:
		rc = getline(&line, &linesize, f);
		if (rc <= 0)
		    die("Unable to read filename for -append");
		if (rc == 1)
		    die("Filename for -append cannot be blank");
		line[rc - 1] = '\0';
		add_append(line, afolder, queue);
		continue;

	    case AMBIGSW:
		ambigsw (line, switches);
		done (1);
	    case UNKWNSW:
		die("%s unknown", line);
	    default:
		die("Switch %s not supported in batch mode", line);
	    }
	} else if (*line == '+') {
	    if (*(line + 1) == '\0')
		die("Invalid null folder name");
	    add_msg(false, NULL, "SELECT \"%s\"", line + 1);
	} else {
	    if (*line == '\0')
		continue;	/* Ignore blank line */
	    add_msg(queue, NULL, "%s", line);
	}
    }

    if (!feof(f)) {
	die("Read of \"%s\" failed: %s", filename, strerror(errno));
    }

    fclose(f);

    if (afolder_alloc)
	free(afolder);

    free(line);
}

/*
 * Negotiate TLS connection, with optional timestamp
 */

static void
imap_negotiate_tls(netsec_context *nsc)
{
    char *errstr;
    struct timeval tv;

    if (timestamp)
	gettimeofday(&tv, NULL);

    if (netsec_negotiate_tls(nsc, &errstr) != OK)
	    die("%s", errstr);

    if (timestamp)
	ts_report(&tv, "TLS negotation time");
}

/*
 * Give a timestamp report.
 */

static void
ts_report(struct timeval *tv, const char *fmt, ...)
{
    struct timeval now;
    double delta;
    va_list ap;

    gettimeofday(&now, NULL);

    delta = ((double) now.tv_sec) - ((double) tv->tv_sec) +
	    (now.tv_usec / 1E6 - tv->tv_usec / 1E6);

    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);

    printf(": %f sec\n", delta);
}

/*
 * Calculate the RFC 822 size of file.
 */

static size_t
rfc822size(const char *filename)
{
    FILE *f;
    size_t total = 0, linecap = 0;
    ssize_t rc;
    char *line = NULL;

    if (! (f = fopen(filename, "r")))
	die("Unable to open %s: %s", filename, strerror(errno));

    while ((rc = getline(&line, &linecap, f)) > 0) {
	total += rc;
	if (line[rc - 1] == '\n' && (rc == 1 || line[rc - 2] != '\r'))
	    total++;
	if (line[rc - 1] != '\n')
	    total += 2;
    }

    free(line);

    if (! feof(f))
	die("Error while reading %s: %s", filename, strerror(errno));

    fclose(f);

    return total;
}
