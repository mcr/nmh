
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
    sasl_conn_t *sasl_conn;	/* SASL connection context */
    sasl_ssf_t sasl_ssf;	/* SASL Security Strength Factor */
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
    SSL_CTX *sslctx;		/* SSL Context */
    SSL *ssl;			/* SSL connection information */
    BIO *ssl_io;		/* BIO used for connection I/O */
#endif /* TLS_SUPPORT */
};

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
 */

netsec_context *
netsec_init(void)
{
    netsec_context *nsc = mh_xmalloc(sizeof(*nsc));

    nsc->ns_fd = -1;
    nsc->ns_snoop = 0;

    return nsc;
}
