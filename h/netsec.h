/* netsec.h -- network-security library routines.
 *
 * These are a common set of routines to handle network security for
 * things like SASL and OpenSSL.
 */

typedef struct _netsec_context netsec_context;

/*
 * Create a network security context.  Returns the allocated network
 * security context.  Cannot fail.
 */

netsec_context *netsec_init(void);

/*
 * Shuts down the security context for a connection and frees all
 * associated resources.  Will unconditionally close the network socket
 * as well.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 */

void netsec_shutdown(netsec_context *ns_context);

/*
 * Sets the file descriptor for this connection.  This will be used by
 * the underlying communication.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * readfd	- Read file descriptor of remote connection.
 * writefd	- Write file descriptor of remote connection
 */

void netsec_set_fd(netsec_context *ns_context, int readfd, int writefd);

/*
 * Set the userid used to authenticate to this connection.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * userid	- Userid to be used for authentication.  Cannot be NULL.
 */

void netsec_set_userid(netsec_context *ns_context, const char *userid);

/*
 * Set the hostname of the server we're connecting to.  This is used
 * by the Cyrus-SASL library and by the TLS code.  This must be called
 * before netsec_set_tls() or netsec_set_sasl_params().
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * hostname	- FQDN of remote host.  Cannot be NULL.
 */

void netsec_set_hostname(netsec_context *ns_context, const char *hostname);

/*
 * Returns "snoop" status on current connection.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 *
 * Returns "1" if snoop is enabled, 0 if it is not.
 */

int netsec_get_snoop(netsec_context *ns_context) PURE;

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
 * A callback designed to handle the snoop output; it can be used by
 * a protocol to massage the data in a more user-friendly way.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * string	- String to output
 * len		- Length of string
 * context	- "Extra" context information to be used by callback.
 */

typedef void (netsec_snoop_callback)(netsec_context *ns_context,
				     const char *string, size_t len,
				     void *context);

/*
 * Set the snoop callback function; will be used to handle protocol-specific
 * messages.  Set to NULL to disable.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * callback	- Snoop callback
 * context	- Extra context information to be passed to callback.
 */

void netsec_set_snoop_callback(netsec_context *ns_context,
			       netsec_snoop_callback *callback, void *context);

/*
 * A sample callback protocols can utilize; decode base64 tokens in the
 * output.  The context is a pointer to an int which contains an offset
 * into the data to start decoding.
 */

extern netsec_snoop_callback netsec_b64_snoop_decoder;

/*
 * Set the read timeout for this connection.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * timeout	- Read timeout, in seconds.
 */

void netsec_set_timeout(netsec_context *ns_context, int timeout);

/*
 * Read a "line" from the network.  This reads one CR/LF terminated line.
 * Returns a pointer to a NUL-terminated string.  This memory is valid
 * until the next call to any read function.  Will return an error if
 * the line does not terminate with a CR/LF.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * length	- Returned length of string
 * errstr	- Error string
 *
 * Returns pointer to string, or NULL on error.
 */

char *netsec_readline(netsec_context *ns_context, size_t *length,
		      char **errstr);

/*
 * Read bytes from the network.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * buffer	- Read buffer
 * size		- Buffer size
 * errstr	- Error size
 *
 * Returns number of bytes read, or -1 on error.
 */

ssize_t netsec_read(netsec_context *ns_context, void *buffer, size_t size,
		    char **errstr);

/*
 * Write data to the network; if encryption is being performed, we will
 * do it.  Data may be buffered; use netsec_flush() to flush any outstanding
 * data to the network.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * buffer	- Output buffer to write to network
 * size		- Size of data to write to network
 * errstr	- Error string
 *
 * Returns OK on success, NOTOK otherwise.
 */

int netsec_write(netsec_context *ns_context, const void *buffer, size_t size,
		 char **errstr);

/*
 * Write bytes using printf formatting
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * errstr	- Error string
 * format	- Format string
 * ...		- Arguments for format string
 *
 * Returns OK on success, NOTOK on error.
 */

int netsec_printf(netsec_context *ns_context, char **errstr,
		  const char *format, ...) CHECK_PRINTF(3, 4);

/*
 * Write bytes using a va_list argument.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * errstr	- Error string
 * format	- Format string
 * ap		- stdarg list.
 *
 * Returns OK on success, NOTOK on error.
 */

int netsec_vprintf(netsec_context *ns_context, char **errstr,
		   const char *format, va_list ap) CHECK_PRINTF(3, 0);

/*
 * Flush any buffered bytes to the network.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * errstr	- Error string
 *
 * Returns OK on success, NOTOK on error.
 */

int netsec_flush(netsec_context *ns_context, char **errstr);

/*
 * Enumerated types for the type of message we are sending/receiving.
 */

enum sasl_message_type {
	NETSEC_SASL_START,	/* Start of SASL authentication */
	NETSEC_SASL_READ,	/* Reading a message */
	NETSEC_SASL_WRITE,	/* Writing a message */
	NETSEC_SASL_FINISH,	/* Complete SASL exchange */
	NETSEC_SASL_CANCEL	/* Cancel a SASL exchange */
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
 * context	- Extra context information potentially required by callback
 * errstr	- An error string to be returned (freed by caller).
 *
 * As a general note, plugins should perform their own I/O.  Buffers returned
 * by NETSEC_SASL_READ should be allocated by the plugins and will be freed
 * by the netsec package.  Error messages returned should be created by
 * netsec_err().
 *
 * Parameter interpretation based on mtype value:
 *
 * NETSEC_SASL_START	- Create a protocol message that starts SASL
 *			  authentication.  If an initial response is
 *			  supported, indata and indatasize will contain it.
 *			  Otherwise they will be set to NULL and 0.
 * NETSEC_SASL_READ	- Parse and decode a protocol message and extract
 *			  out the SASL payload data.  indata will be set
 *			  to NULL; the callback must read in the necessary
 *			  data using the appropriate netsec function.
 *			  outdata/outdatasize should contain the decoded
 *			  SASL message (again, must be free()d by the caller).
 * NETSEC_SASL_WRITE	- Generate a protocol message to send over the
 *			  network.  indata/indatasize will contain the
 *			  SASL payload data.
 * NETSEC_SASL_FINISH	- Process the final SASL message exchange; at
 *			  this point SASL exchange should have completed
 *			  and we should get a message back from the server
 *			  telling us whether or not authentication is
 *			  successful; the plugin should return OK/NOTOK
 *			  to indicate whether or not the authentication
 *			  was successful.  All buffer parameters are NULL.
 * NETSEC_SASL_CANCEL	- Generate a protocol message that cancels the
 *			  SASL protocol exchange; the plugin should
 *			  send this message.  All buffer parameters are NULL.
 *
 * The callback should return OK on success, NOTOK on failure.  Depending
 * at the point of the authentication exchange, the callback may be asked
 * to generate a cancel message.
 */

typedef int (*netsec_sasl_callback)(enum sasl_message_type mtype,
				    unsigned const char *indata,
				    unsigned int indatasize,
				    unsigned char **outdata,
				    unsigned int *outdatasize,
				    void *context, char **errstr);

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
 * mechanism	- The mechanism desired by the user.  If NULL, the SASL
 *		  library will attempt to negotiate the best mechanism.
 * callback	- SASL protocol callbacks 
 * context	- Extra context information passed to the protocol callback
 * errstr	- Error string.
 *
 * Returns NOTOK if SASL is not supported.
 */

int netsec_set_sasl_params(netsec_context *ns_context, const char *service,
			   const char *mechanism,
			   netsec_sasl_callback callback,
			   void *context, char **errstr);

/*
 * Start SASL negotiation.  The Netsec library will use the callbacks
 * supplied in netsec_set_sasl_params() to format and parse the protocol
 * messages.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * mechlist	- Space-separated list of supported SASL mechanisms
 * errstr	- An error string to be returned upon error.
 *
 * Returns OK on success, NOTOK on failure.
 */

int netsec_negotiate_sasl(netsec_context *ns_context, const char *mechlist,
			  char **errstr);

/*
 * Returns the chosen SASL mechanism by the SASL library or netsec.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 *
 * Returns a string containing the chosen mech, or NULL if SASL is not
 * supported or in use.
 */

char *netsec_get_sasl_mechanism(netsec_context *ns_context) PURE;

/*
 * Returns the SASL strength security factor (SSF) for the negotiated
 * authentication mechanism.
 *
 * The exact meaning of the SSF depends on the mechanism chosen, but in
 * general:
 *
 * 0	- No encryption or integrity protection via SASL.
 * 1	- Integrity protection only.
 * >1	- Encryption
 *
 * The SSF is distinct from any encryption that is negotiated by TLS;
 * if TLS is negotiated then the netsec SASL code will automatically disable
 * any attempt to negotiate a security layer and thus the SSF will be 0.
 */

int netsec_get_sasl_ssf(netsec_context *ns_context) PURE;

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
 * TLS negotiation at the appropriate point in the protocol.  The
 * remote hostname (controlled by netsec_set_hostname()) is required
 * to be set before calling this function.
 *
 * Arguments
 *
 * tls		- If nonzero, enable TLS. Otherwise disable TLS
 *		  negotiation.
 * noverify	- If nonzero, disable server certificate and hostname
 *		  validation.
 *
 * Returns NOTOK if TLS is not supported or was unable to initialize.
 */

int netsec_set_tls(netsec_context *context, int tls, int noverify,
		   char **errstr);

/*
 * Start TLS negotiation on this protocol.  This connection should have
 * netsec_set_tls() already called on it.
 *
 * Arguments:
 *
 * ns_context	- Network security context
 * errstr	- Error string upon failure.
 *
 * Returns OK on success, NOTOK on failure.
 */

int netsec_negotiate_tls(netsec_context *ns_context, char **errstr);

/*
 * Allocate and format an error string; should be used by plugins
 * to report errors.
 *
 * Arguments:
 *
 * errstr	- Error string to be returned
 * format	- printf(3) format string
 * ...		- Arguments to printf(3)
 *
 */

void netsec_err(char **errstr, const char *format, ...)
    CHECK_PRINTF(2, 3);
