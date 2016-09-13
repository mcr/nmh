/*
 * Network security library routines for nmh.
 *
 * These are a common set of routines to handle network security for
 * things like SASL and OpenSSL.
 */

struct _netsec_context;
typedef struct _netsec_context netsec_context;

/*
 * Create a network security context.  Returns the allocated network
 * security context.  Cannot fail.
 */

netsec_context *netsec_init(void);

/*
 * Free()s an allocated netsec context and all associated resources.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 */

void netsec_free(netsec_context *ns_context);

/*
 * Sets the file descriptor for this connection.  This will be used by
 * the underlying communication.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * fd		- File descriptor of network connection.
 */

void netset_set_fd(netsec_context *ns_context, int fd);

/*
 * Sets "snoop" status; if snoop is set to a nonzero value, network traffic
 * will be logged on standard error.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * snoop	- Integer value; set to nonzero to enable traffic logging
 */

void netsec_set_snoop(netsec_context *ns_context, int snoop);

/*
 * Enumerated types for the type of message we are sending/receiving.
 */

enum sasl_message_type {
	NETSEC_SASL_START,	/* Start of SASL authentication */
	NETSEC_SASL_READ,	/* Reading a message */
	NETSEC_SASL_WRITE,	/* Writing a message */
	NETSEC_CANCEL		/* Cancel a SASL exchange */
};

/*
 * The SASL callback; this is designed to parse a protocol-specific
 * message and return the SASL protocol message back.
 *
 * The meaning of the arguments to the callback depend on the mtype
 * arguments.  See below for more detail.
 *
 * Arguments:
 *
 * mtype	- The type of message we are processing (read/write/cancel).
 * indata	- A pointer to any input data.
 * indatasize	- The size of the input data in bytes
 * outdata	- Output data (freed by caller)
 * outdatasize	- Size of output data
 * snoop	- If set to true, plugin should log SASL exchange to stderr.
 * errstr	- An error string to be returned (freed by caller).
 *
 * Parameter interpretation based on mtype value:
 *
 * NETSEC_SASL_START	- Create a protocol message that starts SASL
 *			  authentication.  If an initial response is
 *			  supported, indata and indatasize will contain it.
 *			  Otherwise they will be set to NULL and 0.
 *			  The complete protocol message should be
 *			  stored in outdata/outdatasize, to be free()d
 *			  by the caller.
 * NETSEC_SASL_READ	- Parse and decode a protocol message and extract
 *			  out the SASL payload data.  indata will be set
 *			  to NULL; the callback must read in the necessary
 *			  data using the appropriate netsec function.
 *			  outdata/outdatasize should contain the decoded
 *			  SASL message (again, must be free()d by the caller).
 * NETSEC_SASL_WRITE	- Generate a protocol message to send over the
 *			  network.  indata/indatasize will contain the
 *			  SASL payload data.  outdata/outdatasize should
 *			  contain the complete protocol message.
 * NETSEC_SASL_CANCEL	- Generate a protocol message that cancels the
 *			  SASL protocol exchange; outdata/outdatasize
 *			  should contain this message.
 *
 * The callback should return OK on success, NOTOK on failure.  Depending
 * at the point of the authentication exchange, the callback may be asked
 * to generate a cancel message.
 */

typedef int (*netsec_sasl_callback)(enum sasl_message_type mtype,
				    unsigned char *indata,
				    unsigned int indatasize,
				    unsigned char **outdata,
				    unsigned int *outdatasize,
				    int snoop, char **errstr);

/*
 * Sets the SASL parameters for this connection.  If this function is
 * not called or is called with NULL for arguments, SASL authentication
 * will not be attempted for this connection.
 *
 * The caller must provide a callback to parse the protocol and return
 * the SASL protocol messages (see above for callback details).
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * service	- Service name (set to NULL to disable SASL).
 * mechlist	- A space-separated list of mechanisms supported by the server.
 * mechanism	- The mechanism desired by the user.  If NULL, the SASL
 *		  library will attempt to negotiate the best mechanism.
 * callback	- SASL protocol callbacks 
 *
 * Returns NOTOK if SASL is not supported.
 */

int netsec_set_sasl_params(netsec_context *ns_context, const char *service,
			   const char *mechanism,
			   netsec_sasl_callback *callback);

/*
 * Set the OAuth service name used to retrieve the OAuth parameters from
 * user's profile.  Just calling this function is not enough to guarantee
 * that XOAUTH2 authentication will be performed; the appropriate mechanism
 * name must be passed into netsec_set_sasl_params().
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * service	- OAuth2 service names.
 *
 * Returns NOTOK if OAuth2 is not supported.
 */

int netsec_set_oauth_service(netsec_context *ns_context, const char *service);

/*
 * Controls whether or not TLS will be negotiated for this connection.
 *
 * Note: callers still have to call netsec_tls_negotiate() to start
 * TLS negotiation at the appropriate point in the protocol.
 *
 * Arguments
 *
 * tls		- If nonzero, enable TLS. Otherwise disable TLS
 *		  negotiation.
 *
 * Returns NOTOK if TLS is not supported.
 */

int netsec_set_tls(netsec_context *context, int tls);

/*
 * Start TLS negotiation on this protocol.  This connection should have
 * netsec_set_tls() called on it.
 *
 * Arguments:
 *
 * errstr	- Error string upon failure.
 *
 * Returns OK on success, NOTOK on failure.
 */

int netsec_negotiate_tls(char **errstr);
