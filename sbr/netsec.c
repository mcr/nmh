
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
#include <h/oauth.h>
#include <stdarg.h>
#include <sys/select.h>

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

#define SASL_MAXRECVBUF 65536
#endif /* CYRUS_SASL */

#ifdef TLS_SUPPORT
#include <openssl/ssl.h>
#include <openssl/err.h>

static int tls_initialized = 0;
static SSL_CTX *sslctx = NULL;		/* SSL Context */

#endif /* TLS_SUPPORT */

/* I'm going to hardcode this for now; maybe make it adjustable later? */
#define NETSEC_BUFSIZE 65536

/*
 * Our context structure, which holds all of the relevant information
 * about a connection.
 */

struct _netsec_context {
    int ns_readfd;		/* Read descriptor for network connection */
    int ns_writefd;		/* Write descriptor for network connection */
    int ns_snoop;		/* If true, display network data */
    int ns_snoop_noend;		/* If true, didn't get a CR/LF on last line */
    netsec_snoop_callback *ns_snoop_cb; /* Snoop output callback */
    void *ns_snoop_context;	/* Context data for snoop function */
    int ns_timeout;		/* Network read timeout, in seconds */
    char *ns_userid;		/* Userid for authentication */
    unsigned char *ns_inbuffer;	/* Our read input buffer */
    unsigned char *ns_inptr;	/* Our read buffer input pointer */
    unsigned int ns_inbuflen;	/* Length of data in input buffer */
    unsigned int ns_inbufsize;	/* Size of input buffer */
    unsigned char *ns_outbuffer;/* Output buffer */
    unsigned char *ns_outptr;	/* Output buffer pointer */
    unsigned int ns_outbuflen;	/* Output buffer data length */
    unsigned int ns_outbufsize;	/* Output buffer size */
    char *sasl_mech;		/* User-requested mechanism */
#ifdef OAUTH_SUPPORT
    char *oauth_service;	/* OAuth2 service name */
#endif /* OAUTH_SUPPORT */
#ifdef CYRUS_SASL
    char *sasl_hostname;	/* Hostname we've connected to */
    sasl_conn_t *sasl_conn;	/* SASL connection context */
    sasl_ssf_t sasl_ssf;	/* SASL Security Strength Factor */
    netsec_sasl_callback sasl_proto_cb; /* SASL callback we use */
    sasl_callback_t *sasl_cbs;	/* Callbacks used by SASL */
    nmh_creds_t sasl_creds;	/* Credentials (username/password) */
    sasl_secret_t *sasl_secret;	/* SASL password structure */
    char *sasl_chosen_mech;	/* Mechanism chosen by SASL */
    int sasl_seclayer;		/* If true, SASL security layer is enabled */
    char *sasl_tmpbuf;		/* Temporary read buffer for decodes */
    size_t sasl_maxbufsize;	/* Maximum negotiated SASL buffer size */
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
    BIO *ssl_io;		/* BIO used for connection I/O */
    int tls_active;		/* If true, TLS is running */
#endif /* TLS_SUPPORT */
};

/*
 * Function to read data from the actual network socket
 */

static int netsec_fillread(netsec_context *ns_context, char **errstr);

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

    nsc->ns_readfd = -1;
    nsc->ns_writefd = -1;
    nsc->ns_snoop = 0;
    nsc->ns_snoop_noend = 0;
    nsc->ns_snoop_cb = NULL;
    nsc->ns_snoop_context = NULL;
    nsc->ns_userid = NULL;
    nsc->ns_timeout = 60;	/* Our default */
    nsc->ns_inbufsize = NETSEC_BUFSIZE;
    nsc->ns_inbuffer = mh_xmalloc(nsc->ns_inbufsize);
    nsc->ns_inptr = nsc->ns_inbuffer;
    nsc->ns_inbuflen = 0;
    nsc->ns_outbufsize = NETSEC_BUFSIZE;
    nsc->ns_outbuffer = mh_xmalloc(nsc->ns_outbufsize);
    nsc->ns_outptr = nsc->ns_outbuffer;
    nsc->ns_outbuflen = 0;
    nsc->sasl_mech = NULL;
#ifdef OAUTH_SUPPORT
    nsc->oauth_service = NULL;
#endif /* OAUTH_SUPPORT */
#ifdef CYRUS_SASL
    nsc->sasl_conn = NULL;
    nsc->sasl_hostname = NULL;
    nsc->sasl_cbs = NULL;
    nsc->sasl_creds = NULL;
    nsc->sasl_secret = NULL;
    nsc->sasl_chosen_mech = NULL;
    nsc->sasl_ssf = 0;
    nsc->sasl_seclayer = 0;
    nsc->sasl_tmpbuf = NULL;
    nsc->sasl_maxbufsize = 0;
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
    nsc->ssl_io = NULL;
    nsc->tls_active = 0;
#endif /* TLS_SUPPORT */
    return nsc;
}

/*
 * Shutdown the connection completely and free all resources.
 * The connection is only closed if the flag is given.
 */

void
netsec_shutdown(netsec_context *nsc, int closeflag)
{
    if (nsc->ns_userid)
	free(nsc->ns_userid);
    if (nsc->ns_inbuffer)
	free(nsc->ns_inbuffer);
    if (nsc->ns_outbuffer)
	free(nsc->ns_outbuffer);
    if (nsc->sasl_mech)
	free(nsc->sasl_mech);
#ifdef OAUTH_SERVICE
    if (nsc->oauth_service)
	free(nsc->oauth_service);
#endif /* OAUTH_SERVICE */
#ifdef CYRUS_SASL
    if (nsc->sasl_conn)
	sasl_dispose(&nsc->sasl_conn);
    if (nsc->sasl_hostname)
	free(nsc->sasl_hostname);
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
    if (nsc->sasl_tmpbuf)
	free(nsc->sasl_tmpbuf);
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
    if (nsc->ssl_io)
	/*
	 * I checked; BIO_free_all() will cause SSL_shutdown to be called
	 * on the SSL object in the chain.
	 */
	BIO_free_all(nsc->ssl_io);
#endif /* TLS_SUPPORT */

    if (closeflag) {
	if (nsc->ns_readfd != -1)
	    close(nsc->ns_readfd);
	if (nsc->ns_writefd != -1 && nsc->ns_writefd != nsc->ns_readfd)
	    close(nsc->ns_writefd);
    }

    free(nsc);
}

/*
 * Set the file descriptor for our context
 */

void
netsec_set_fd(netsec_context *nsc, int readfd, writefd)
{
    nsc->ns_readfd = readfd;
    nsc->ns_writefd = writefd;
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
 * Get the snoop flag for this connection
 */

int
netsec_get_snoop(netsec_context *nsc)
{
    return nsc->ns_snoop;
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
 * Set the snoop callback for this connection.
 */

void netsec_set_snoop_callback(netsec_context *nsc,
			       netsec_snoop_callback callback, void *context)
{
    nsc->ns_snoop_cb = callback;
    nsc->ns_snoop_context = context;
}

/*
 * A base64-decoding snoop callback
 */

void
netsec_b64_snoop_decoder(netsec_context *nsc, const char *string, size_t len,
			 void *context)
{
    const char *decoded;
    size_t decodedlen;

    int offset = context ? *((int *) context) : 0;

    if (offset > 0) {
	/*
	 * Output non-base64 data first.
	 */
	fprintf(stderr, "%.*s", offset, string);
	string += offset;
	len -= offset;
    }

    if (decodeBase64(string, &decoded, &decodedlen, 1, NULL) == OK) {
	char *hexified;
	hexify((const unsigned char *) decoded, decodedlen, &hexified);
	fprintf(stderr, "b64<%s>\n", hexified);
	free(hexified);
	free((char *) decoded);
    } else {
	fprintf(stderr, "%.*s\n", (int) len, string);
    }
}

/*
 * Set the read timeout for this connection
 */

void
netsec_set_timeout(netsec_context *nsc, int timeout)
{
    nsc->ns_timeout = timeout;
}

/*
 * Read data from the network.  Basically, return anything in our buffer,
 * otherwise fill from the network.
 */

ssize_t
netsec_read(netsec_context *nsc, void *buffer, size_t size, char **errstr)
{
    int retlen;

    /*
     * If our buffer is empty, then we should fill it now
     */

    if (nsc->ns_inbuflen == 0) {
	if (netsec_fillread(nsc, errstr) != OK)
	    return NOTOK;
    }

    /*
     * netsec_fillread only returns if the buffer is full, so we can
     * assume here that this has something in it.
     */

    retlen = size > nsc->ns_inbuflen ? nsc->ns_inbuflen : size;

    memcpy(buffer, nsc->ns_inptr, retlen);

    if (retlen == (int) nsc->ns_inbuflen) {
	/*
	 * We've emptied our buffer, so reset everything.
	 */
	nsc->ns_inptr = nsc->ns_inbuffer;
	nsc->ns_inbuflen = 0;
    } else {
	nsc->ns_inptr += size;
	nsc->ns_inbuflen -= size;
    }

    return OK;
}

/*
 * Get a "line" (CR/LF) terminated from the network.
 *
 * Okay, we play some games here, so pay attention:
 *
 * - Unlike every other function, we return a pointer to the
 *   existing buffer.  This pointer is valid until you call another
 *   read functiona again.
 * - We NUL-terminated the buffer right at the end, before the terminator.
 * - Technically we look for a LF; if we find a CR right before it, then
 *   we back up one.
 * - If your data may contain embedded NULs, this won't work.
 */

char *
netsec_readline(netsec_context *nsc, size_t *len, char **errstr)
{
    unsigned char *ptr = nsc->ns_inptr;
    size_t count = 0, offset;

retry:
    /*
     * Search through our existing buffer for a LF
     */

    while (count < nsc->ns_inbuflen) {
	count++;
	if (*ptr++ == '\n') {
	    char *sptr = (char *) nsc->ns_inptr;
	    if (count > 1 && *(ptr - 2) == '\r')
		ptr--;
	    *--ptr = '\0';
	    if (len)
		*len = ptr - nsc->ns_inptr;
	    nsc->ns_inptr += count;
	    nsc->ns_inbuflen -= count;
	    if (nsc->ns_snoop) {
#ifdef CYRUS_SASL
		if (nsc->sasl_seclayer)
		    fprintf(stderr, "(sasl-encrypted) ");
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
		if (nsc->tls_active)
		    fprintf(stderr, "(tls-encrypted) ");
#endif /* TLS_SUPPORT */
		fprintf(stderr, "<= ");
		if (nsc->ns_snoop_cb)
		    nsc->ns_snoop_cb(nsc, sptr, strlen(sptr),
				     nsc->ns_snoop_context);
		else
		    fprintf(stderr, "%s\n", sptr);
	    }
	    return sptr;
	}
    }

    /*
     * Hm, we didn't find a \n.  If we've already searched half of the input
     * buffer, return an error.
     */

    if (count >= nsc->ns_inbufsize / 2) {
	netsec_err(errstr, "Unable to find a line terminator after %d bytes",
		   count);
	return NULL;
    }

    /*
     * Okay, get some more network data.  This may move inptr, so regenerate
     * our ptr value;
     */

    offset = ptr - nsc->ns_inptr;

    if (netsec_fillread(nsc, errstr) != OK)
	return NULL;

    ptr = nsc->ns_inptr + offset;

    goto retry;

    return NULL;	/* Should never reach this */
}

/*
 * Fill our read buffer with some data from the network.
 */

static int
netsec_fillread(netsec_context *nsc, char **errstr)
{
    unsigned char *end;
    char *readbuf;
    size_t readbufsize, remaining, startoffset;
    int rc;

    /*
     * If inbuflen is zero, that means the buffer has been emptied
     * completely.  In that case move inptr back to the start.
     */

    if (nsc->ns_inbuflen == 0) {
	nsc->ns_inptr = nsc->ns_inbuffer;
    }

retry:
    /*
     * If we are using TLS and there's anything pending, then skip the
     * select call
     */
#ifdef TLS_SUPPORT
    if (!nsc->tls_active || BIO_pending(nsc->ssl_io) == 0)
#endif /* TLS_SUPPORT */
    {
	struct timeval tv;
	fd_set rfds;

	FD_ZERO(&rfds);
	FD_SET(nsc->ns_readfd, &rfds);

	tv.tv_sec = nsc->ns_timeout;
	tv.tv_usec = 0;

	rc = select(nsc->ns_readfd + 1, &rfds, NULL, NULL, &tv);

	if (rc == -1) {
	    netsec_err(errstr, "select() while reading failed: %s",
		       strerror(errno));
	    return NOTOK;
	}

	if (rc == 0) {
	    netsec_err(errstr, "read() timed out after %d seconds",
		       nsc->ns_timeout);
	    return NOTOK;
	}

	/*
	 * At this point, we know that rc is 1, so there's not even any
	 * point to check to see if our descriptor is set in rfds.
	 */
    }

    startoffset = nsc->ns_inptr - nsc->ns_inbuffer;
    remaining = nsc->ns_inbufsize - (startoffset + nsc->ns_inbuflen);
    end = nsc->ns_inptr + nsc->ns_inbuflen;

    /*
     * If we are using TLS, then just read via the BIO.  But we still
     * use our local buffer.
     */
#ifdef TLS_SUPPORT
    if (nsc->tls_active) {
	rc = BIO_read(nsc->ssl_io, end, remaining);
	if (rc == 0) {
	    /*
	     * Either EOF, or possibly an error.  Either way, it was probably
	     * unexpected, so treat as error.
	     */
	    netsec_err(errstr, "TLS peer aborted connection");
	    return NOTOK;
	} else if (rc < 0) {
	    /* Definitely an error */
	    netsec_err(errstr, "Read on TLS connection failed: %s",
		       ERR_error_string(ERR_get_error(), NULL));
	    return NOTOK;
	}

	nsc->ns_inbuflen += rc;

	return OK;
    }
#endif /* TLS_SUPPORT */

    /*
     * Okay, time to read some data.  Either we're just doing it straight
     * or we're passing it through sasl_decode() first.
     */

#ifdef CYRUS_SASL
    if (nsc->sasl_seclayer) {
	readbuf = nsc->sasl_tmpbuf;
	readbufsize = nsc->sasl_maxbufsize;
    } else
#endif /* CYRUS_SASL */
    {
	readbuf = (char *) end;
	readbufsize = remaining;
    }

    /*
     * At this point, we should have active data on the connection (see
     * select() above) so this read SHOULDN'T block.  Hopefully.
     */

    rc = read(nsc->ns_readfd, readbuf, readbufsize);

    if (rc == 0) {
	netsec_err(errstr, "Received EOF on network read");
	return NOTOK;
    }

    if (rc < 0) {
	netsec_err(errstr, "Network read failed: %s", strerror(errno));
	return NOTOK;
    }

    /*
     * Okay, so we've had a successful read.  If we are doing SASL security
     * layers, pass this through sasl_decode().  sasl_decode() can return
     * 0 bytes decoded; if that happens, jump back to the beginning.  Otherwise
     * we can just update our length pointer.
     */

#ifdef CYRUS_SASL
    if (nsc->sasl_seclayer) {
	const char *tmpout;
	unsigned int tmpoutlen;

	rc = sasl_decode(nsc->sasl_conn, nsc->sasl_tmpbuf, rc,
			 &tmpout, &tmpoutlen);

	if (rc != SASL_OK) {
	    netsec_err(errstr, "Unable to decode SASL network data: %s",
		       sasl_errdetail(nsc->sasl_conn));
	    return NOTOK;
	}

	if (tmpoutlen == 0)
	    goto retry;

	/*
	 * Just in case ...
	 */

	if (tmpoutlen > remaining) {
	    netsec_err(errstr, "Internal error: SASL decode buffer overflow!");
	    return NOTOK;
	}

	memcpy(end, tmpout, tmpoutlen);

	nsc->ns_inbuflen += tmpoutlen;
    } else
#endif /* CYRUS_SASL */
	nsc->ns_inbuflen += rc;

    /*
     * If we're past the halfway point in our read buffers, shuffle everything
     * back to the beginning.
     */

    if (startoffset > nsc->ns_inbufsize / 2) {
	memmove(nsc->ns_inbuffer, nsc->ns_inptr, nsc->ns_inbuflen);
	nsc->ns_inptr = nsc->ns_inbuffer;
    }

    return OK;
}

/*
 * Write data to our network connection.  Really, fill up the buffer as
 * much as we can, and flush it out if necessary.  netsec_flush() does
 * the real work.
 */

int
netsec_write(netsec_context *nsc, const void *buffer, size_t size,
	     char **errstr)
{
    const unsigned char *bufptr = buffer;
    int rc, remaining;

    /* Just in case */

    if (size == 0)
	return OK;

    /*
     * If TLS is active, then bypass all of our buffering logic; just
     * write it directly to our BIO.  We have a buffering BIO first in
     * our stack, so buffering will take place there.
     */
#ifdef TLS_SUPPORT
    if (nsc->tls_active) {
	rc = BIO_write(nsc->ssl_io, buffer, size);

	if (rc <= 0) {
	    netsec_err(errstr, "Error writing to TLS connection: %s",
		       ERR_error_string(ERR_get_error(), NULL));
	    return NOTOK;
	}

	return OK;
    }
#endif /* TLS_SUPPORT */

    /*
     * Run a loop copying in data to our local buffer; when we're done with
     * any buffer overflows then just copy any remaining data in.
     */

    while ((int) size >= (remaining = nsc->ns_outbufsize - nsc->ns_outbuflen)) {
	memcpy(nsc->ns_outptr, bufptr, remaining);

	/*
	 * In theory I should increment outptr, but netsec_flush just resets
	 * it anyway.
	 */
	nsc->ns_outbuflen = nsc->ns_outbufsize;

	rc = netsec_flush(nsc, errstr);

	if (rc != OK)
	    return NOTOK;

	bufptr += remaining;
	size -= remaining;
    }

    /*
     * Copy any leftover data into the buffer.
     */

    if (size > 0) {
	memcpy(nsc->ns_outptr, bufptr, size);
	nsc->ns_outptr += size;
	nsc->ns_outbuflen += size;
    }

    return OK;
}

/*
 * Our network printf() routine, which really just calls netsec_vprintf().
 */

int
netsec_printf(netsec_context *nsc, char **errstr, const char *format, ...)
{
    va_list ap;
    int rc;

    va_start(ap, format);
    rc = netsec_vprintf(nsc, errstr, format, ap);
    va_end(ap);

    return rc;
}

/*
 * Write bytes to the network using printf()-style formatting.
 *
 * Again, for the most part copy stuff into our buffer to be flushed
 * out later.
 */

int
netsec_vprintf(netsec_context *nsc, char **errstr, const char *format,
	       va_list ap)
{
    int rc;

    /*
     * Again, if we're using TLS, then bypass our local buffering
     */
#ifdef TLS_SUPPORT
    if (nsc->tls_active) {
	rc = BIO_vprintf(nsc->ssl_io, format, ap);

	if (rc <= 0) {
	    netsec_err(errstr, "Error writing to TLS connection: %s",
		       ERR_error_string(ERR_get_error(), NULL));
	    return NOTOK;
	}

	return OK;
    }
#endif /* TLS_SUPPORT */

    /*
     * Cheat a little.  If we can fit the data into our outgoing buffer,
     * great!  If not, generate a flush and retry once.
     */

retry:
    rc = vsnprintf((char *) nsc->ns_outptr,
		   nsc->ns_outbufsize - nsc->ns_outbuflen, format, ap);

    if (rc >= (int) (nsc->ns_outbufsize - nsc->ns_outbuflen)) {
	/*
	 * This means we have an overflow.  Note that we don't actually
	 * make use of the terminating NUL, but according to the spec
	 * vsnprintf() won't write to the last byte in the string; that's
	 * why we have to use >= in the comparison above.
	 */
	if (nsc->ns_outbuffer == nsc->ns_outptr) {
	    /*
	     * Whoops, if the buffer pointer was the same as the start of the
	     * buffer, that means we overflowed the internal buffer.
	     * At that point, just give up.
	     */
	    netsec_err(errstr, "Internal error: wanted to printf() a total of "
	    	       "%d bytes, but our buffer size was only %d bytes",
		       rc, nsc->ns_outbufsize);
	    return NOTOK;
	} else {
	    /*
	     * Generate a flush (which may be inefficient, but hopefully
	     * it isn't) and then try again.
	     */
	    if (netsec_flush(nsc, errstr) != OK)
		return NOTOK;
	    /*
	     * After this, outbuffer should == outptr, so we shouldn't
	     * hit this next time around.
	     */
	    goto retry;
	}
    }

    if (nsc->ns_snoop) {
	int outlen = rc;
	if (outlen > 0 && nsc->ns_outptr[outlen - 1] == '\n') {
	    outlen--;
	    if (outlen > 0 && nsc->ns_outptr[outlen - 1] == '\r')
		outlen--;
	} else {
	    nsc->ns_snoop_noend = 1;
	}
	if (outlen > 0 || nsc->ns_snoop_noend == 0) {
#ifdef CYRUS_SASL
	    if (nsc->sasl_seclayer)
		fprintf(stderr, "(sasl-encrypted) ");
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
	    if (nsc->tls_active)
		fprintf(stderr, "(tls-encrypted) ");
#endif /* TLS_SUPPORT */
	    fprintf(stderr, "=> ");
	    if (nsc->ns_snoop_cb)
		nsc->ns_snoop_cb(nsc, nsc->ns_outptr, outlen,
				 nsc->ns_snoop_context);
	    else
		 fprintf(stderr, "%.*s\n", outlen, nsc->ns_outptr); 
	} else {
	    nsc->ns_snoop_noend = 0;
	}
    }

    nsc->ns_outptr += rc;
    nsc->ns_outbuflen += rc;

    return OK;
}

/*
 * Flush out any buffered data in our output buffers.  This routine is
 * actually where the real network writes take place.
 */

int
netsec_flush(netsec_context *nsc, char **errstr)
{
    const char *netoutbuf = (const char *) nsc->ns_outbuffer;
    unsigned int netoutlen = nsc->ns_outbuflen;
    int rc;

    /*
     * For TLS connections, just call BIO_flush(); we'll let TLS handle
     * all of our output buffering.
     */
#ifdef TLS_SUPPORT
    if (nsc->tls_active) {
	rc = BIO_flush(nsc->ssl_io);

	if (rc <= 0) {
	    netsec_err(errstr, "Error flushing TLS connection: %s",
		       ERR_error_string(ERR_get_error(), NULL));
	    return NOTOK;
	}

	return OK;
    }
#endif /* TLS_SUPPORT */

    /*
     * Small optimization
     */

    if (netoutlen == 0)
	return OK;

    /*
     * If SASL security layers are in effect, run the data through
     * sasl_encode() first and then write it.
     */
#ifdef CYRUS_SASL
    if (nsc->sasl_seclayer) {
	rc = sasl_encode(nsc->sasl_conn, (const char *) nsc->ns_outbuffer,
			 nsc->ns_outbuflen, &netoutbuf, &netoutlen);

	if (rc != SASL_OK) {
	    netsec_err(errstr, "SASL data encoding failed: %s",
		       sasl_errdetail(nsc->sasl_conn));
	    return NOTOK;
	}

    }
#endif /* CYRUS_SASL */
    rc = write(nsc->ns_writefd, netoutbuf, netoutlen);

    if (rc < 0) {
	netsec_err(errstr, "write() failed: %s", strerror(errno));
	return NOTOK;
    }

    nsc->ns_outptr = nsc->ns_outbuffer;
    nsc->ns_outbuflen = 0;

    return OK;
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
	    netsec_err(errstr, "SASL client initialization failed: %s",
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
	netsec_err(errstr, "SASL new client allocation failed: %s",
		   sasl_errstring(retval, NULL, NULL));
	return NOTOK;
    }

    nsc->sasl_mech = mechanism ? getcpy(mechanism) : NULL;
    nsc->sasl_proto_cb = callback;
    nsc->sasl_hostname = getcpy(hostname);

    return OK;
#else /* CYRUS_SASL */
    netsec_err(errstr, "SASL is not supported");

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

	if (nmh_get_credentials(nsc->sasl_hostname, nsc->ns_userid, 1,
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

	if (nmh_get_credentials(nsc->sasl_hostname, nsc->ns_userid, 0,
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
    const char *chosen_mech;
    const unsigned char *saslbuf;
    unsigned char *outbuf;
    unsigned int saslbuflen, outbuflen;
    sasl_ssf_t *ssf;
    int *outbufmax;
#endif
#ifdef OAUTH_SUPPORT
    unsigned char *xoauth_client_res;
    size_t xoauth_client_res_len;
#endif /* OAUTH_SUPPORT */
    int rc;

    /*
     * If we've been passed a requested mechanism, check our mechanism
     * list from the protocol.  If it's not supported, return an error.
     */

    if (nsc->sasl_mech) {
	char **str, *mlist = getcpy(mechlist);
	int i;

	str = brkstring(mlist, " ", NULL);

	for (i = 0; str[i] != NULL; i++) {
	    if (strcasecmp(nsc->sasl_mech, str[i]) == 0) {
		break;
	    }
	}

	i = (str[i] == NULL);

	free(str);
	free(mlist);

	if (i) {
	    netsec_err(errstr, "Chosen mechanism %s not supported by server",
		       nsc->sasl_mech);
	    return NOTOK;
	}
    }

#ifdef OAUTH_SUPPORT
    if (nsc->sasl_mech && strcasecmp(nsc->sasl_mech, "XOAUTH2") == 0) {
	/*
	 * This should be relatively straightforward, but requires some
	 * help from the plugin.  Basically, if XOAUTH2 is a success,
	 * the callback has to return success, but no output data.  If
	 * there is output data, it will be assumed that it is the JSON
	 * error message.
	 */

	if (! nsc->oauth_service) {
	    netsec_err(errstr, "Internal error: OAuth2 service name not given");
	    return NOTOK;
	}

	nsc->sasl_chosen_mech = getcpy(nsc->sasl_mech);

	if (mh_oauth_do_xoauth(nsc->ns_userid, nsc->oauth_service,
			       &xoauth_client_res, &xoauth_client_res_len,
			       nsc->ns_snoop ? stderr : NULL) != OK) {
	    netsec_err(errstr, "Internal error: Unable to get OAuth2 "
	    	       "bearer token");
	    return NOTOK;
	}

	rc = nsc->sasl_proto_cb(NETSEC_SASL_START, xoauth_client_res,
				xoauth_client_res_len, NULL, 0, errstr);
	free(xoauth_client_res);

	if (rc != OK)
	    return NOTOK;

	/*
	 * Okay, we need to do a NETSEC_SASL_FINISH now.  If we return
	 * success, we indicate that with no output data.  But if we
	 * fail, then send a blank message and get the resulting
	 * error.
	 */

	rc = nsc->sasl_proto_cb(NETSEC_SASL_FINISH, NULL, 0, NULL, 0, errstr);

	if (rc != OK) {
	    /*
	     * We're going to assume the error here is a JSON response;
	     * we ignore it and send a blank message in response.  We should
	     * then get either an +OK or -ERR
	     */
	    free(errstr);
	    nsc->sasl_proto_cb(NETSEC_SASL_WRITE, NULL, 0, NULL, 0, NULL);
	    rc = nsc->sasl_proto_cb(NETSEC_SASL_FINISH, NULL, 0, NULL, 0,
				    errstr);
	    if (rc == 0) {
		netsec_err(errstr, "Unexpected success after OAuth failure!");
	    }
	    return NOTOK;
	}
	return OK;
    }
#endif /* OAUTH_SUPPORT */

#ifdef CYRUS_SASL
    /*
     * In netsec_set_sasl_params, we've already done all of our setup with
     * sasl_client_init() and sasl_client_new().  So time to set security
     * properties, call sasl_client_start(), and generate the protocol
     * messages.
     */

    memset(&secprops, 0, sizeof(secprops));
    secprops.maxbufsize = SASL_MAXRECVBUF;

    /*
     * If we're using TLS, do not negotiate a security layer
     */

    secprops.max_ssf = 
#ifdef TLS_SUPPORT
		nsc->tls_active ? 0 :
#endif /* TLS_SUPPORT */
		UINT_MAX;

    rc = sasl_setprop(nsc->sasl_conn, SASL_SEC_PROPS, &secprops);

    if (rc != SASL_OK) {
	netsec_err(errstr, "SASL security property initialization failed: %s",
		   sasl_errstring(rc, NULL, NULL));
	return NOTOK;
    }

    /*
     * Start the actual protocol negotiation, and go through the
     * sasl_client_step() loop (after sasl_client_start, of course).
     */

    rc = sasl_client_start(nsc->sasl_conn,
    			   nsc->sasl_mech ? nsc->sasl_mech : mechlist, NULL,
			   (const char **) &saslbuf, &saslbuflen,
			   &chosen_mech);

    if (rc != SASL_OK && rc != SASL_CONTINUE) {
	netsec_err(errstr, "SASL client start failed: %s",
		   sasl_errdetail(nsc->sasl_conn));
	return NOTOK;
    }

    nsc->sasl_chosen_mech = getcpy(chosen_mech);

    if (nsc->sasl_proto_cb(NETSEC_SASL_START, saslbuf, saslbuflen, NULL, 0,
			   errstr) != OK)
	return NOTOK;

    /*
     * We've written out our first message; enter in the step loop
     */

    while (rc == SASL_CONTINUE) {
    	/*
	 * Call our SASL callback, which will handle the details of
	 * reading data from the network.
	 */

	if (nsc->sasl_proto_cb(NETSEC_SASL_READ, NULL, 0, &outbuf, &outbuflen,
			       errstr) != OK) {
	    nsc->sasl_proto_cb(NETSEC_SASL_CANCEL, NULL, 0, NULL, 0, NULL);
	    return NOTOK;
	}

	rc = sasl_client_step(nsc->sasl_conn, (char *) outbuf, outbuflen, NULL,
			      (const char **) &saslbuf, &saslbuflen);

	if (outbuf)
	    free(outbuf);

	if (rc != SASL_OK && rc != SASL_CONTINUE) {
	    netsec_err(errstr, "SASL client negotiation failed: %s",
		       sasl_errdetail(nsc->sasl_conn));
	    nsc->sasl_proto_cb(NETSEC_SASL_CANCEL, NULL, 0, NULL, 0, NULL);
	    return NOTOK;
	}

	if (nsc->sasl_proto_cb(NETSEC_SASL_WRITE, saslbuf, saslbuflen,
			       NULL, 0, errstr) != OK) {
	    nsc->sasl_proto_cb(NETSEC_SASL_CANCEL, NULL, 0, NULL, 0, NULL);
	    return NOTOK;
	}
    }

    /*
     * SASL exchanges should be complete, process the final response message
     * from the server.
     */

    if (nsc->sasl_proto_cb(NETSEC_SASL_FINISH, NULL, 0, NULL, 0,
			   errstr) != OK) {
	/*
	 * At this point we can't really send an abort since the SASL dialog
	 * has completed, so just bubble back up the error message.
	 */

	return NOTOK;
    }

    /*
     * At this point, SASL should be complete.  Get a few properties
     * from the authentication exchange.
     */

    rc = sasl_getprop(nsc->sasl_conn, SASL_SSF, (const void **) &ssf);

    if (rc != SASL_OK) {
	netsec_err(errstr, "Cannot retrieve SASL negotiated security "
		  "strength factor: %s", sasl_errstring(rc, NULL, NULL));
	return NOTOK;
    }

    nsc->sasl_ssf = *ssf;

    if (nsc->sasl_ssf > 0) {
	rc = sasl_getprop(nsc->sasl_conn, SASL_MAXOUTBUF,
			  (const void **) &outbufmax);

	if (rc != SASL_OK) {
	    netsec_err(errstr, "Cannot retrieve SASL negotiated output "
		       "buffer size: %s", sasl_errstring(rc, NULL, NULL));
	    return NOTOK;
	}

	/*
	 * If our output buffer isn't the same size as the input buffer,
	 * reallocate it and set the new size (since we won't encode any
	 * data larger than that).
	 */

	nsc->sasl_maxbufsize = *outbufmax;

	if (nsc->ns_outbufsize != nsc->sasl_maxbufsize) {
	    nsc->ns_outbufsize = nsc->sasl_maxbufsize;
	    nsc->ns_outbuffer = mh_xrealloc(nsc->ns_outbuffer,
					    nsc->ns_outbufsize);
	    /*
	     * There shouldn't be any data in the buffer, but for
	     * consistency's sake discard it.
	     */
	    nsc->ns_outptr = nsc->ns_outbuffer;
	    nsc->ns_outbuflen = 0;
	}

	/*
	 * Allocate a buffer to do temporary reads into, before we
	 * call sasl_decode()
	 */

	nsc->sasl_tmpbuf = mh_xmalloc(nsc->sasl_maxbufsize);

	/*
	 * Okay, this is a bit weird.  Make sure that the input buffer
	 * is at least TWICE the size of the max buffer size.  That's
	 * because if we're consuming data but want to extend the current
	 * buffer, we want to be sure there's room for another full buffer's
	 * worth of data.
	 */

	if (nsc->ns_inbufsize < nsc->sasl_maxbufsize * 2) {
	    size_t offset = nsc->ns_inptr - nsc->ns_inbuffer;
	    nsc->ns_inbufsize = nsc->sasl_maxbufsize * 2;
	    nsc->ns_inbuffer = mh_xrealloc(nsc->ns_inbuffer, nsc->ns_inbufsize);
	    nsc->ns_inptr = nsc->ns_inbuffer + offset;
	}

	nsc->sasl_seclayer = 1;
    }

    return OK;
#else
    netsec_err(errstr, "SASL not supported");

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
 * Set an OAuth2 service name, if we support it.
 */

int
netsec_set_oauth_service(netsec_context *nsc, const char *service)
{
#ifdef OAUTH_SUPPORT
    nsc->oauth_service = getcpy(service);
    return OK;
#else /* OAUTH_SUPPORT */
    return NOTOK;
#endif /* OAUTH_SUPPORT */
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
	BIO *rbio, *wbio, *ssl_bio;;

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
		netsec_err(errstr, "Unable to initialize OpenSSL context: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return NOTOK;
	    }

	    SSL_CTX_set_options(sslctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
				SSL_OP_NO_TLSv1);

	    tls_initialized++;
	}

	if (nsc->ns_readfd == -1 || nsc->ns_writefd == -1) {
	    netsec_err(errstr, "Invalid file descriptor in netsec context");
	    return NOTOK;
	}

	/*
	 * Create the SSL structure which holds the data for a single
	 * TLS connection.
	 */

	ssl = SSL_new(sslctx);

	if (! ssl) {
	    netsec_err(errstr, "Unable to create SSL connection: %s",
		       ERR_error_string(ERR_get_error(), NULL));
	    return NOTOK;
	}

	/*
	 * Never bother us, since we are using blocking sockets.
	 */

	SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

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

	rbio = BIO_new_socket(nsc->ns_readfd, BIO_NOCLOSE);

	if (! rbio) {
	    netsec_err(errstr, "Unable to create a read socket BIO: %s",
		       ERR_error_string(ERR_get_error(), NULL));
	    SSL_free(ssl);
	    return NOTOK;
	}

	wbio = BIO_new_socket(nsc->ns_writefd, BIO_NOCLOSE);

	if (! wbio) {
	    netsec_err(errstr, "Unable to create a write socket BIO: %s",
		       ERR_error_string(ERR_get_error(), NULL));
	    SSL_free(ssl);
	    BIO_free(rbio);
	    return NOTOK;
	}

	SSL_set_bio(ssl, rbio, wbio);
	SSL_set_connect_state(ssl);

	nsc->ssl_io = BIO_new(BIO_f_buffer());

	if (! nsc->ssl_io) {
	    netsec_err(errstr, "Unable to create a buffer BIO: %s",
		       ERR_error_string(ERR_get_error(), NULL));
	    SSL_free(ssl);
	    return NOTOK;
	}

	ssl_bio = BIO_new(BIO_f_ssl());

	if (! ssl_bio) {
	    netsec_err(errstr, "Unable to create a SSL BIO: %s",
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
	netsec_err(errstr, "TLS is not supported");

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
	netsec_err(errstr, "TLS has not been configured for this connection");
	return NOTOK;
    }

    if (BIO_do_handshake(nsc->ssl_io) < 1) {
	netsec_err(errstr, "TLS negotiation failed: %s",
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

    /*
     * At this point, TLS has been activated; we're not going to use
     * the output buffer, so free it now to save a little bit of memory.
     */

    if (nsc->ns_outbuffer) {
	free(nsc->ns_outbuffer);
	nsc->ns_outbuffer = NULL;
    }

    return OK;
#else /* TLS_SUPPORT */
    netsec_err(errstr, "TLS not supported");

    return NOTOK;
#endif /* TLS_SUPPORT */
}

/*
 * Generate an (allocated) error string
 */

void
netsec_err(char **errstr, const char *fmt, ...)
{
    va_list ap;
    size_t errbufsize;
    char *errbuf = NULL;
    int rc = 127;

    if (! errstr)
    	return;

    do {
	errbufsize = rc + 1;
	errbuf = mh_xrealloc(errbuf, errbufsize);
	va_start(ap, fmt);
	rc = vsnprintf(errbuf, errbufsize, fmt, ap);
	va_end(ap);
    } while (rc >= (int) errbufsize);

    *errstr = errbuf;
}
