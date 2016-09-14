
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
    unsigned char *ns_inbuffer;	/* Our read input buffer */
    unsigned char *ns_inptr;	/* Our read buffer input pointer */
    unsigned int ns_inbuflen;	/* Length of data in input buffer */
    unsigned int ns_inbufsize;	/* Size of input buffer */
    unsigned char *ns_outbuffer;/* Output buffer */
    unsigned char *ns_outptr;	/* Output buffer pointer */
    unsigned int ns_outbuflen;	/* Output buffer data length */
    unsigned int ns_outbufsize;	/* Output buffer size */
#ifdef CYRUS_SASL
    char *sasl_service;		/* SASL service name */
    char *sasl_mech;		/* User-requested mechanism */
    sasl_conn_t *sasl_conn;	/* SASL connection context */
    sasl_ssf_t sasl_ssf;	/* SASL Security Strength Factor */
    netsec_sasl_callback sasl_cb; /* SASL callback */
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
    nsc->ns_inbuffer = nsc->ns_inptr = NULL;
    nsc->ns_inbuflen = nsc->ns_inbufsize = 0;
    nsc->ns_outbuffer = nsc->ns_outptr = NULL;
    nsc->ns_outbuflen = nsc->ns_outbufsize = 0;
#ifdef CYRUS_SASL
    nsc->sasl_conn = NULL;
    nsc->sasl_service = nsc->sasl_mech = NULL;
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
    nsc->ssl_io = NULL;
    nsc->tls_active = 0;
#endif /* TLS_SUPPORT */
    return nsc;
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
 * Set the snoop flag for this connection
 */

void
netsec_set_snoop(netsec_context *nsc, int snoop)
{
    nsc->ns_snoop = snoop;
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
