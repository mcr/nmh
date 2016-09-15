
/*
 * netsec.c -- Network security routines for handling protocols that
 *	       require SASL and/or TLS.
 *
 * This code is Copyright (c) 2016, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <h/netsec.h>
#include <stdarg.h>

#ifdef CYRUS_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
# if SASL_VERSION_FULL < 0x020125
  /* Cyrus SASL 2.1.25 introduced the sasl_callback_ft prototype,
     which has an explicit void parameter list, according to best
     practice.  So we need to cast to avoid compile warnings.
     Provide this prototype for earlier versions. */
  typedef int (*sasl_callback_ft)();
# endif /* SASL_VERSION_FULL < 0x020125 */

static int netsec_get_user(void *context, int id, const char **result,
			   unsigned int *len);
static int netsec_get_password(sasl_conn_t *conn, void *context, int id,
			       sasl_secret_t **psecret);

static int sasl_initialized = 0;
#endif /* CYRUS_SASL */

#ifdef TLS_SUPPORT
#include <openssl/ssl.h>
#include <openssl/err.h>

static int tls_initialized = 0;
static SSL_CTX *sslctx = NULL;		/* SSL Context */

#endif /* TLS_SUPPORT */

/*
 * Our context structure, which holds all of the relevant information
 * about a connection.
 */

struct _netsec_context {
    int ns_fd;			/* Descriptor for network connection */
    int ns_snoop;		/* If true, display network data */
    char *ns_hostname;		/* Hostname we've connected to */
    char *ns_userid;		/* Userid for authentication */
    unsigned char *ns_inbuffer;	/* Our read input buffer */
    unsigned char *ns_inptr;	/* Our read buffer input pointer */
    unsigned int ns_inbuflen;	/* Length of data in input buffer */
    unsigned int ns_inbufsize;	/* Size of input buffer */
    unsigned char *ns_outbuffer;/* Output buffer */
    unsigned char *ns_outptr;	/* Output buffer pointer */
    unsigned int ns_outbuflen;	/* Output buffer data length */
    unsigned int ns_outbufsize;	/* Output buffer size */
#ifdef CYRUS_SASL
    char *sasl_mech;		/* User-requested mechanism */
    sasl_conn_t *sasl_conn;	/* SASL connection context */
    sasl_ssf_t sasl_ssf;	/* SASL Security Strength Factor */
    netsec_sasl_callback sasl_proto_cb; /* SASL callback we use */
    sasl_callback_t *sasl_cbs;	/* Callbacks used by SASL */
    nmh_creds_t sasl_creds;	/* Credentials (username/password) */
    sasl_secret_t *sasl_secret;	/* SASL password structure */
    char *sasl_chosen_mech;	/* Mechanism chosen by SASL */
    int sasl_enabled;		/* If true, SASL is enabled */
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
    BIO *ssl_io;		/* BIO used for connection I/O */
    int tls_active;		/* If true, TLS is running */
#endif /* TLS_SUPPORT */
};

/*
 * Function to allocate error message strings
 */

static char *netsec_errstr(const char *format, ...);

/*
 * How this code works, in general.
 *
 * _If_ we are using no encryption or SASL encryption, then we buffer the
 * network data through ns_inbuffer and ns_outbuffer.  That should be
 * relatively self-explanatory.
 *
 * If we are using SSL for encryption, then use a buffering BIO for output
 * (that just easier).  Still do buffering for reads; when we need more
 * data we call the BIO_read() function to fill our local buffer.
 *
 * For SASL, we make use of (for now) the Cyrus-SASL library.  For some
 * mechanisms, we implement those mechanisms directly since the Cyrus SASL
 * library doesn't support them (like OAuth).
 */

/*
 * Allocate and initialize our security context
 */

netsec_context *
netsec_init(void)
{
    netsec_context *nsc = mh_xmalloc(sizeof(*nsc));

    nsc->ns_fd = -1;
    nsc->ns_snoop = 0;
    nsc->ns_userid = nsc->ns_hostname = NULL;
    nsc->ns_inbuffer = nsc->ns_inptr = NULL;
    nsc->ns_inbuflen = nsc->ns_inbufsize = 0;
    nsc->ns_outbuffer = nsc->ns_outptr = NULL;
    nsc->ns_outbuflen = nsc->ns_outbufsize = 0;
#ifdef CYRUS_SASL
    nsc->sasl_conn = NULL;
    nsc->sasl_mech = NULL;
    nsc->sasl_cbs = NULL;
    nsc->sasl_creds = NULL;
    nsc->sasl_secret = NULL;
    nsc->sasl_chosen_mech = NULL;
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
    nsc->ssl_io = NULL;
    nsc->tls_active = 0;
#endif /* TLS_SUPPORT */
    return nsc;
}

/*
 * Shutdown the connection completely and free all resources.
 * The connection is not closed, however.
 */

void
netsec_shutdown(netsec_context *nsc)
{
    if (nsc->ns_hostname)
	free(nsc->ns_hostname);
    if (nsc->ns_userid)
	free(nsc->ns_userid);
    if (nsc->ns_inbuffer)
	free(nsc->ns_inbuffer);
    if (nsc->ns_outbuffer)
	free(nsc->ns_outbuffer);
#ifdef CYRUS_SASL
    if (nsc->sasl_conn)
	sasl_dispose(&nsc->sasl_conn);
    if (nsc->sasl_mech)
	free(nsc->sasl_mech);
    if (nsc->sasl_cbs)
	free(nsc->sasl_cbs);
    if (nsc->sasl_creds) {
	if (nsc->sasl_creds->password)
	    memset(nsc->sasl_creds->password, 0,
	    	   strlen(nsc->sasl_creds->password));
	free(nsc->sasl_creds);
    }
    if (nsc->sasl_secret) {
	if (nsc->sasl_secret->len > 0) {
	    memset(nsc->sasl_secret->data, 0, nsc->sasl_secret->len);
	}
	free(nsc->sasl_secret);
    }
    if (nsc->sasl_chosen_mech)
	free(nsc->sasl_chosen_mech);
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
    if (nsc->ssl_io)
	/*
	 * I checked; BIO_free_all() will cause SSL_shutdown to be called
	 * on the SSL object in the chain.
	 */
	BIO_free_all(nsc->ssl_io);
#endif /* TLS_SUPPORT */

    free(nsc);
}

/*
 * Set the file descriptor for our context
 */

void
netsec_set_fd(netsec_context *nsc, int fd)
{
    nsc->ns_fd = fd;
}

/*
 * Set the remote hostname we've connected to.
 */

void
netsec_set_hostname(netsec_context *nsc, const char *hostname)
{
    nsc->ns_hostname = getcpy(hostname);
}

/*
 * Set the userid used for authentication for this context
 */

void
netsec_set_userid(netsec_context *nsc, const char *userid)
{
    nsc->ns_userid = getcpy(userid);
}

/*
 * Set the snoop flag for this connection
 */

void
netsec_set_snoop(netsec_context *nsc, int snoop)
{
    nsc->ns_snoop = snoop;
}

/*
 * Set various SASL protocol parameters
 */

int
netsec_set_sasl_params(netsec_context *nsc, const char *hostname,
		       const char *service, const char *mechanism,
		       netsec_sasl_callback callback, char **errstr)
{
#ifdef CYRUS_SASL
    sasl_callback_t *sasl_cbs;
    int retval;

    if (! sasl_initialized) {
	retval = sasl_client_init(NULL);
	if (retval != SASL_OK) {
	    *errstr = netsec_errstr("SASL client initialization failed: %s",
				    sasl_errstring(retval, NULL, NULL));
	    return NOTOK;
	}
	sasl_initialized++;
    }

    /*
     * Allocate an array of SASL callbacks for this connection.
     * Right now we just allocate an array of four callbacks.
     */

    sasl_cbs = mh_xmalloc(sizeof(*sasl_cbs) * 4);

    sasl_cbs[0].id = SASL_CB_USER;
    sasl_cbs[0].proc = (sasl_callback_ft) netsec_get_user;
    sasl_cbs[0].context = nsc;

    sasl_cbs[1].id = SASL_CB_AUTHNAME;
    sasl_cbs[1].proc = (sasl_callback_ft) netsec_get_user;
    sasl_cbs[1].context = nsc;

    sasl_cbs[2].id = SASL_CB_PASS;
    sasl_cbs[2].proc = (sasl_callback_ft) netsec_get_password;
    sasl_cbs[2].context = nsc;

    sasl_cbs[3].id = SASL_CB_LIST_END;
    sasl_cbs[3].proc = NULL;
    sasl_cbs[3].context = NULL;

    nsc->sasl_cbs = sasl_cbs;

    retval = sasl_client_new(service, hostname, NULL, NULL, nsc->sasl_cbs, 0,
    			     &nsc->sasl_conn);

    if (retval) {
	*errstr = netsec_errstr("SASL new client allocation failed: %s",
				sasl_errstring(retval, NULL, NULL));
	return NOTOK;
    }

    nsc->sasl_mech = mechanism ? getcpy(mechanism) : NULL;
    nsc->sasl_proto_cb = callback;

    return OK;
#else /* CYRUS_SASL */
    *errstr = netsec_errstr("SASL is not supported");

    return NOTOK;
#endif /* CYRUS_SASL */
}

#ifdef CYRUS_SASL
/*
 * Our userid callback; return the specified username to the SASL
 * library when asked.
 */

int netsec_get_user(void *context, int id, const char **result,
		    unsigned int *len)
{
    netsec_context *nsc = (netsec_context *) context;

    if (! result || (id != SASL_CB_USER && id != SASL_CB_AUTHNAME))
	return SASL_BADPARAM;

    if (nsc->ns_userid == NULL) {
	/*
	 * Pass the 1 third argument to nmh_get_credentials() so that
	 * a defauly user if the -user switch wasn't supplied, and so
	 * that a default password will be supplied.  That's used when
	 * those values really don't matter, and only with legacy/.netrc,
	 * i.e., with a credentials profile entry.
	 */

	if (nsc->sasl_creds == NULL) {
	    nsc->sasl_creds = mh_xmalloc(sizeof(*nsc->sasl_creds));
	    nsc->sasl_creds->user = NULL;
	    nsc->sasl_creds->password = NULL;
	}

	if (nmh_get_credentials(nsc->ns_hostname, nsc->ns_userid, 1,
				nsc->sasl_creds) != OK)
	    return SASL_BADPARAM;

	if (nsc->ns_userid != nsc->sasl_creds->user) {
	    if (nsc->ns_userid)
		free(nsc->ns_userid);
	    nsc->ns_userid = getcpy(nsc->sasl_creds->user);
	}
    }

    *result = nsc->ns_userid;
    if (len)
	*len = strlen(nsc->ns_userid);

    return SASL_OK;
}

/*
 * Retrieve a password and return it to SASL
 */

static int
netsec_get_password(sasl_conn_t *conn, void *context, int id,
		    sasl_secret_t **psecret)
{
    netsec_context *nsc = (netsec_context *) context;
    int len;

    NMH_UNUSED(conn);

    if (! psecret || id != SASL_CB_PASS)
	return SASL_BADPARAM;

    if (nsc->sasl_creds == NULL) {
	nsc->sasl_creds = mh_xmalloc(sizeof(*nsc->sasl_creds));
	nsc->sasl_creds->user = NULL;
	nsc->sasl_creds->password = NULL;
    }

    if (nsc->sasl_creds->password == NULL) {
	/*
	 * Pass the 0 third argument to nmh_get_credentials() so
	 * that the default password isn't used.  With legacy/.netrc
	 * credentials support, we'll only get here if the -user
	 * switch to send(1)/post(8) wasn't used.
	 */

	if (nmh_get_credentials(nsc->ns_hostname, nsc->ns_userid, 0,
				nsc->sasl_creds) != OK) {
	    return SASL_BADPARAM;
	}
    }

    len = strlen(nsc->sasl_creds->password);

    /*
     * sasl_secret_t includes 1 bytes for "data" already, so that leaves
     * us room for a terminating NUL
     */

    *psecret = (sasl_secret_t *) malloc(sizeof(sasl_secret_t) + len);

    if (! *psecret)
	return SASL_NOMEM;

    (*psecret)->len = len;
    strcpy((char *) (*psecret)->data, nsc->sasl_creds->password);

    nsc->sasl_secret = *psecret;

    return SASL_OK;
}
#endif /* CYRUS_SASL */

/*
 * Negotiate SASL on this connection
 */

int
netsec_negotiate_sasl(netsec_context *nsc, const char *mechlist, char **errstr)
{
#ifdef CYRUS_SASL
    sasl_security_properties_t secprops;
    char *chosen_mech;
    sasl_ssf_t *ssf;
    int rc;

    return OK;
#else
    *errstr = netsec_errstr("SASL not supported");

    return NOTOK;
#endif /* CYRUS_SASL */
}

/*
 * Retrieve our chosen SASL mechanism
 */

char *
netsec_get_sasl_mechanism(netsec_context *nsc)
{
#ifdef CYRUS_SASL
    return nsc->sasl_chosen_mech;
#else /* CYRUS_SASL */
    return NULL;
#endif /* CYRUS_SASL */
}

/*
 * Initialize (and enable) TLS for this connection
 */

int
netsec_set_tls(netsec_context *nsc, int tls, char **errstr)
{
    if (tls) {
#ifdef TLS_SUPPORT
	SSL *ssl;
	BIO *sbio, *ssl_bio;;

	if (! tls_initialized) {
	    SSL_library_init();
	    SSL_load_error_strings();

	    /*
	     * Create the SSL context; this has the properties for all
	     * SSL connections (we are only doing one), though.  Make sure
	     * we only support secure (known as of now) TLS protocols.
	     */

	    sslctx = SSL_CTX_new(SSLv23_client_method());

	    if (! sslctx) {
		*errstr = netsec_errstr("Unable to initialize OpenSSL "
					"context: %s",
					ERR_error_string(ERR_get_error(), NULL));
		return NOTOK;
	    }

	    SSL_CTX_set_options(sslctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
				SSL_OP_NO_TLSv1);

	    tls_initialized++;
	}

	if (nsc->ns_fd == -1) {
	    *errstr = netsec_errstr("Invalid file descriptor in netsec "
	    			    "context");
	    return NOTOK;
	}

	/*
	 * Create the SSL structure which holds the data for a single
	 * TLS connection.
	 */

	ssl = SSL_new(sslctx);

	if (! ssl) {
	    *errstr = netsec_errstr("Unable to create SSL connection: %s",
	    			    ERR_error_string(ERR_get_error(), NULL));
	    return NOTOK;
	}

	/*
	 * This is a bit weird, so pay attention.
	 *
	 * We create a socket BIO, and bind it to our SSL connection.
	 * That means reads and writes to the SSL connection will use our
	 * supplied socket.
	 *
	 * Then we create an SSL BIO, and assign our current SSL connection
	 * to it.  We then create a buffer BIO and push it in front of our
	 * SSL BIO.  So the chain looks like:
	 *
	 * buffer BIO -> SSL BIO -> socket BIO.
	 *
	 * So writes and reads are buffered (we mostly care about writes).
	 */

	sbio = BIO_new_socket(nsc->ns_fd, BIO_NOCLOSE);

	if (! sbio) {
	    *errstr = netsec_errstr("Unable to create a socket BIO: %s",
	    			    ERR_error_string(ERR_get_error(), NULL));
	    SSL_free(ssl);
	    return NOTOK;
	}

	SSL_set_bio(ssl, sbio, sbio);
	SSL_set_connect_state(ssl);

	nsc->ssl_io = BIO_new(BIO_f_buffer());

	if (! nsc->ssl_io) {
	    *errstr = netsec_errstr("Unable to create a buffer BIO: %s",
	    			    ERR_error_string(ERR_get_error(), NULL));
	    SSL_free(ssl);
	    return NOTOK;
	}

	ssl_bio = BIO_new(BIO_f_ssl());

	if (! ssl_bio) {
	    *errstr = netsec_errstr("Unable to create a SSL BIO: %s",
	    			    ERR_error_string(ERR_get_error(), NULL));
	    SSL_free(ssl);
	    return NOTOK;
	}

	BIO_set_ssl(ssl_bio, ssl, BIO_CLOSE);
	BIO_push(nsc->ssl_io, ssl_bio);

	return OK;
    } else {
	BIO_free_all(nsc->ssl_io);
	nsc->ssl_io = NULL;

	return OK;
    }
#else /* TLS_SUPPORT */
	*errstr = netsec_errstr("TLS is not supported");

	return NOTOK;
    }
#endif /* TLS_SUPPORT */
}

/*
 * Start TLS negotiation on this connection
 */

int
netsec_negotiate_tls(netsec_context *nsc, char **errstr)
{
#ifdef TLS_SUPPORT
    if (! nsc->ssl_io) {
	*errstr = netsec_errstr("TLS has not been configured for this "
				"connection");
	return NOTOK;
    }

    if (BIO_do_handshake(nsc->ssl_io) < 1) {
	*errstr = netsec_errstr("TLS negotiation failed: %s",
				ERR_error_string(ERR_get_error(), NULL));
	return NOTOK;
    }

    if (nsc->ns_snoop) {
	SSL *ssl;

	if (BIO_get_ssl(nsc->ssl_io, &ssl) < 1) {
	    fprintf(stderr, "WARNING: cannot determine SSL ciphers\n");
	} else {
	    const SSL_CIPHER *cipher = SSL_get_current_cipher(ssl);
	    fprintf(stderr, "TLS negotation successful: %s(%d) %s\n",
		    SSL_CIPHER_get_name(cipher),
		    SSL_CIPHER_get_bits(cipher, NULL),
		    SSL_CIPHER_get_version(cipher));
	}
    }

    nsc->tls_active = 1;

    return OK;
#else /* TLS_SUPPORT */
    *errstr = netsec_errstr("TLS not supported");

    return NOTOK;
#endif /* TLS_SUPPORT */
}

/*
 * Generate an (allocated) error string
 */

static char *
netsec_errstr(const char *fmt, ...)
{
    va_list ap;
    size_t errbufsize;
    char *errbuf = NULL;
    int rc = 127;

    do {
	errbufsize = rc + 1;
	errbuf = mh_xrealloc(errbuf, errbufsize);
	va_start(ap, fmt);
	rc = vsnprintf(errbuf, errbufsize, fmt, ap);
	va_end(ap);
    } while (rc >= (int) errbufsize);

    return errbuf;
}
