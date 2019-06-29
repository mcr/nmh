/* netsec.c -- Network security routines for handling protocols that
 *	       require SASL and/or TLS.
 *
 * This code is Copyright (c) 2016, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "credentials.h"
#include "getcpy.h"
#include "brkstring.h"
#include "h/utils.h"
#include "h/netsec.h"
#include "h/oauth.h"
#include <stdarg.h>
#include <sys/select.h>
#include "base64.h"

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

static bool sasl_initialized;

#define SASL_MAXRECVBUF 65536
#endif /* CYRUS_SASL */

#ifdef TLS_SUPPORT
#include <openssl/ssl.h>
#include <openssl/err.h>

static bool tls_initialized;
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
    int ns_noclose;		/* Do not close file descriptors if set */
    int ns_snoop;		/* If true, display network data */
    netsec_snoop_callback *ns_snoop_cb; /* Snoop output callback */
    void *ns_snoop_context;	/* Context data for snoop function */
    char *ns_snoop_savebuf;	/* Save buffer for snoop data */
    int ns_timeout;		/* Network read timeout, in seconds */
    char *ns_userid;		/* Userid for authentication */
    char *ns_hostname;		/* Hostname we've connected to */
    unsigned char *ns_inbuffer;	/* Our read input buffer */
    unsigned char *ns_inptr;	/* Our read buffer input pointer */
    unsigned int ns_inbuflen;	/* Length of data in input buffer */
    unsigned int ns_inbufsize;	/* Size of input buffer */
    unsigned char *ns_outbuffer;/* Output buffer */
    unsigned char *ns_outptr;	/* Output buffer pointer */
    unsigned int ns_outbuflen;	/* Output buffer data length */
    unsigned int ns_outbufsize;	/* Output buffer size */
    char *sasl_mech;		/* User-requested mechanism */
    char *sasl_chosen_mech;	/* Mechanism chosen by SASL */
    netsec_sasl_callback sasl_proto_cb; /* SASL callback we use */
    void *sasl_proto_context;	/* Context to be used by SASL callback */
#ifdef OAUTH_SUPPORT
    char *oauth_service;	/* OAuth2 service name */
#endif /* OAUTH_SUPPORT */
#ifdef CYRUS_SASL
    sasl_conn_t *sasl_conn;	/* SASL connection context */
    sasl_ssf_t sasl_ssf;	/* SASL Security Strength Factor */
    sasl_callback_t *sasl_cbs;	/* Callbacks used by SASL */
    nmh_creds_t sasl_creds;	/* Credentials (username/password) */
    sasl_secret_t *sasl_secret;	/* SASL password structure */
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
 * Code to check the ASCII content of a byte array.
 */

static int checkascii(const unsigned char *byte, size_t len);

/*
 * How this code works, in general.
 *
 * _If_ we are using no encryption then we buffer the network data
 * through ns_inbuffer and ns_outbuffer.  That should be relatively
 * self-explanatory.
 *
 * If we use encryption, then ns_inbuffer and ns_outbuffer contain the
 * cleartext data.  When it comes time to send the encrypted data on the
 * (either from a flush or the buffer is full) we either use BIO_write()
 * for TLS or sasl_encode() (followed by a write() for Cyrus-SASL.  For
 * reads we either use BIO_read() (TLS) or do a network read into a
 * temporary buffer and use sasl_decode() (Cyrus-SASL).  Note that if
 * negotiate TLS then we disable SASL encryption.
 *
 * We used to use a buffering BIO for the reads/writes for TLS, but it
 * ended up being complicated to special-case the buffering for everything
 * except TLS, so the buffering is now unified, no matter which encryption
 * method is being used (even none).
 *
 * For SASL authentication, we make use of (for now) the Cyrus-SASL
 * library.  For some mechanisms, we implement those mechanisms directly
 * since the Cyrus SASL library doesn't support them (like OAuth).
 */

/*
 * Allocate and initialize our security context
 */

netsec_context *
netsec_init(void)
{
    netsec_context *nsc;

    NEW(nsc);
    nsc->ns_readfd = -1;
    nsc->ns_writefd = -1;
    nsc->ns_noclose = 0;
    nsc->ns_snoop = 0;
    nsc->ns_snoop_cb = NULL;
    nsc->ns_snoop_context = NULL;
    nsc->ns_snoop_savebuf = NULL;
    nsc->ns_userid = NULL;
    nsc->ns_hostname = NULL;
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
    nsc->sasl_chosen_mech = NULL;
    nsc->sasl_proto_cb = NULL;
    nsc->sasl_proto_context = NULL;
#ifdef OAUTH_SUPPORT
    nsc->oauth_service = NULL;
#endif /* OAUTH_SUPPORT */
#ifdef CYRUS_SASL
    nsc->sasl_conn = NULL;
    nsc->sasl_cbs = NULL;
    nsc->sasl_creds = NULL;
    nsc->sasl_secret = NULL;
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
 */

void
netsec_shutdown(netsec_context *nsc)
{
    free(nsc->ns_userid);
    free(nsc->ns_hostname);
    free(nsc->ns_inbuffer);
    free(nsc->ns_outbuffer);
    free(nsc->sasl_mech);
    free(nsc->sasl_chosen_mech);
    free(nsc->ns_snoop_savebuf);
#ifdef OAUTH_SERVICE
    free(nsc->oauth_service);
#endif /* OAUTH_SERVICE */
#ifdef CYRUS_SASL
    if (nsc->sasl_conn)
	sasl_dispose(&nsc->sasl_conn);
    free(nsc->sasl_cbs);
    if (nsc->sasl_creds)
	nmh_credentials_free(nsc->sasl_creds);
    if (nsc->sasl_secret) {
	if (nsc->sasl_secret->len > 0) {
	    memset(nsc->sasl_secret->data, 0, nsc->sasl_secret->len);
	}
	free(nsc->sasl_secret);
    }
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

    if (! nsc->ns_noclose) {
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
netsec_set_fd(netsec_context *nsc, int readfd, int writefd)
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
 * Set the hostname of the remote host we're connecting to.
 */

void
netsec_set_hostname(netsec_context *nsc, const char *hostname)
{
    nsc->ns_hostname = mh_xstrdup(hostname);
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

void
netsec_set_snoop_callback(netsec_context *nsc,
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
    unsigned char *decoded;
    size_t decodedlen;
    int offset;
    NMH_UNUSED(nsc);

    offset = context ? *((int *) context) : 0;

    if (offset > 0) {
	/*
	 * Output non-base64 data first.
	 */
	fprintf(stderr, "%.*s", offset, string);
	string += offset;
	len -= offset;
    }

    if (decodeBase64(string, &decoded, &decodedlen, 1) == OK) {
	/*
	 * Some mechanisms produce large binary tokens, which aren't really
	 * readable.  So let's do a simple heuristic.  If the token is greater
	 * than 100 characters _and_ the first 100 bytes are more than 50%
	 * non-ASCII, then don't print the decoded buffer, just the
	 * base64 text.
	 */
	if (decodedlen > 100 && !checkascii(decoded, 100)) {
	    fprintf(stderr, "%.*s\n", (int) len, string);
	} else {
	    char *hexified;
	    hexify(decoded, decodedlen, &hexified);
	    fprintf(stderr, "b64<%s>\n", hexified);
	    free(hexified);
	}
	free(decoded);
    } else {
	fprintf(stderr, "%.*s\n", (int) len, string);
    }
}

/*
 * If the ASCII content is > 50%, return 1
 */

static int
checkascii(const unsigned char *bytes, size_t len)
{
    size_t count = 0, half = len / 2;

    while (len-- > 0) {
	if (isascii(*bytes) && isprint(*bytes) && ++count > half)
	    return 1;
	bytes++;
	/* No chance by this point */
	if (count + len < half)
	    return 0;
    }

    return 0;
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

    retlen = min(size, nsc->ns_inbuflen);

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
 *   read function again.
 * - We NUL-terminate the buffer right at the end, before the CR-LF terminator.
 * - Technically we look for a LF; if we find a CR right before it, then
 *   we back up one.
 * - If your data may contain embedded NULs, this won't work.  You should
 *   be using netsec_read() in that case.
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
		    fprintf(stderr, "(sasl-decrypted) ");
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
		if (nsc->tls_active)
		    fprintf(stderr, "(tls-decrypted) ");
#endif /* TLS_SUPPORT */
		fprintf(stderr, "<= ");
		if (nsc->ns_snoop_cb)
		    nsc->ns_snoop_cb(nsc, sptr, strlen(sptr),
				     nsc->ns_snoop_context);
		else {
                    fputs(sptr, stderr);
                    putc('\n', stderr);
                }
	    }
	    return sptr;
	}
    }

    /*
     * Hm, we didn't find a \n.  If we've already searched half of the input
     * buffer, return an error.
     */

    if (count >= nsc->ns_inbufsize / 2) {
	netsec_err(errstr, "Unable to find a line terminator after %zu bytes",
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

#if defined(CYRUS_SASL) || defined(TLS_SUPPORT)
retry:
#endif /* CYRUS_SASL || TLS_SUPPORT */
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

    /*
     * Some explanation:
     *
     * startoffset is the offset from the beginning of the input
     * buffer to data that is in our input buffer, but has not yet
     * been consumed.  This can be non-zero if functions like
     * netsec_readline() leave leftover data.
     *
     * remaining is the remaining amount of unconsumed data in the input
     * buffer.
     *
     * end is a pointer to the end of the valid data + 1; it's where
     * the next read should go.
     */

    startoffset = nsc->ns_inptr - nsc->ns_inbuffer;
    remaining = nsc->ns_inbufsize - (startoffset + nsc->ns_inbuflen);
    end = nsc->ns_inptr + nsc->ns_inbuflen;

    /*
     * If we're past the halfway point in our read buffers, shuffle everything
     * back to the beginning.
     */

    if (startoffset > nsc->ns_inbufsize / 2) {
	memmove(nsc->ns_inbuffer, nsc->ns_inptr, nsc->ns_inbuflen);
	nsc->ns_inptr = nsc->ns_inbuffer;
	startoffset = 0;
	remaining = nsc->ns_inbufsize - nsc->ns_inbuflen;
	end = nsc->ns_inptr + nsc->ns_inbuflen;
    }

    /*
     * If we are using TLS, then just read via the BIO.  But we still
     * use our local buffer.
     */
#ifdef TLS_SUPPORT
    if (nsc->tls_active) {
	rc = BIO_read(nsc->ssl_io, end, remaining);
	if (rc == 0) {
	    SSL *ssl;
	    int errcode;

	    /*
	     * Check to see if we're supposed to retry; if so,
	     * then go back and read again.
	     */

	    if (BIO_should_retry(nsc->ssl_io))
		goto retry;

	    /*
	     * Okay, fine.  Get the real error out of the SSL context.
	     */

	    if (BIO_get_ssl(nsc->ssl_io, &ssl) < 1) {
		netsec_err(errstr, "SSL_read() returned 0, but cannot "
			   "retrieve SSL context");
		return NOTOK;
	    }

	    errcode = SSL_get_error(ssl, rc);
	    if (errcode == SSL_ERROR_ZERO_RETURN) {
		netsec_err(errstr, "TLS peer closed remote connection");
	    } else {
		netsec_err(errstr, "TLS network read failed: %s",
			   ERR_error_string(ERR_peek_last_error(), NULL));
	    }
	    if (nsc->ns_snoop)
		ERR_print_errors_fp(stderr);
	    return NOTOK;
	}
        if (rc < 0) {
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
    va_list apcopy;

    /*
     * Cheat a little.  If we can fit the data into our outgoing buffer,
     * great!  If not, generate a flush and retry once.
     */

retry:
    va_copy(apcopy, ap);
    rc = vsnprintf((char *) nsc->ns_outptr,
		   nsc->ns_outbufsize - nsc->ns_outbuflen, format, apcopy);
    va_end(apcopy);

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
	}
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
     * Small optimization
     */

    if (netoutlen == 0)
	return OK;

    /*
     * If we have snoop turned on, output the data.
     *
     * Note here; if we don't have a CR or LF at the end, save the data
     * in ns_snoop_savebuf for later and print it next time.
     */

    if (nsc->ns_snoop) {
	unsigned int snoopoutlen = netoutlen;
	const char *snoopoutbuf = (const char *) nsc->ns_outbuffer;

	while (snoopoutlen > 0) {
	    const char *end = strpbrk(snoopoutbuf, "\r\n");
	    unsigned int outlen;

	    if (! end) {
		if (nsc->ns_snoop_savebuf) {
		    nsc->ns_snoop_savebuf = mh_xrealloc(nsc->ns_snoop_savebuf,
						strlen(nsc->ns_snoop_savebuf) +
						snoopoutlen + 1);
		    strncat(nsc->ns_snoop_savebuf, snoopoutbuf, snoopoutlen);
		} else {
		    nsc->ns_snoop_savebuf = mh_xmalloc(snoopoutlen + 1);
		    strncpy(nsc->ns_snoop_savebuf, snoopoutbuf, snoopoutlen);
		    nsc->ns_snoop_savebuf[snoopoutlen] = '\0';
		}
		break;
	    }

	    outlen = end - snoopoutbuf;

#ifdef CYRUS_SASL
	    if (nsc->sasl_seclayer)
		fprintf(stderr, "(sasl-encrypted) ");
#endif /* CYRUS_SASL */
#ifdef TLS_SUPPORT
	    if (nsc->tls_active)
		fprintf(stderr, "(tls-encrypted) ");
#endif /* TLS_SUPPORT */
	    fprintf(stderr, "=> ");
	    if (nsc->ns_snoop_cb) {
		const char *ptr;
		unsigned int cb_len = outlen;

		if (nsc->ns_snoop_savebuf) {
		    cb_len += strlen(nsc->ns_snoop_savebuf);
		    nsc->ns_snoop_savebuf = mh_xrealloc(nsc->ns_snoop_savebuf,
							outlen);
		    ptr = nsc->ns_snoop_savebuf;
		} else {
		    ptr = snoopoutbuf;
		}

		nsc->ns_snoop_cb(nsc, ptr, cb_len, nsc->ns_snoop_context);

		if (nsc->ns_snoop_savebuf) {
		    free(nsc->ns_snoop_savebuf);
		    nsc->ns_snoop_savebuf = NULL;
		}
	    } else {
		if (nsc->ns_snoop_savebuf) {
		    fprintf(stderr, "%s", nsc->ns_snoop_savebuf);
		    free(nsc->ns_snoop_savebuf);
		    nsc->ns_snoop_savebuf = NULL;
		}
		fprintf(stderr, "%.*s\n", outlen, snoopoutbuf);
	    }

	    /*
	     * Alright, hopefully any previous leftover data is done,
	     * and we have the current line output.  Move things past the
	     * next CR/LF.
	     */

	    snoopoutlen -= outlen;
	    snoopoutbuf += outlen;

	    if (snoopoutlen > 0 && *snoopoutbuf == '\r') {
		snoopoutlen--;
		snoopoutbuf++;
	    }

	    if (snoopoutlen > 0 && *snoopoutbuf == '\n') {
		snoopoutlen--;
		snoopoutbuf++;
	    }
	}
    }

    /*
     * If SASL security layers are in effect, run the data through
     * sasl_encode() first.
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

    /*
     * If TLS is active, then use those functions to write out the
     * data.
     */
#ifdef TLS_SUPPORT
    if (nsc->tls_active) {
	if (BIO_write(nsc->ssl_io, netoutbuf, netoutlen) <= 0) {
	    netsec_err(errstr, "Error writing to TLS connection: %s",
		       ERR_error_string(ERR_get_error(), NULL));
	    return NOTOK;
	}
    } else
#endif /* TLS_SUPPORT */
    {
	rc = write(nsc->ns_writefd, netoutbuf, netoutlen);

	if (rc < 0) {
	    netsec_err(errstr, "write() failed: %s", strerror(errno));
	    return NOTOK;
	}
    }

    nsc->ns_outptr = nsc->ns_outbuffer;
    nsc->ns_outbuflen = 0;

    return OK;
}

/*
 * Set various SASL protocol parameters
 */

int
netsec_set_sasl_params(netsec_context *nsc, const char *service,
		       const char *mechanism, netsec_sasl_callback callback,
		       void *context, char **errstr)
{
#ifdef CYRUS_SASL
    sasl_callback_t *sasl_cbs;
    int retval;

    if (!nsc->ns_hostname) {
	netsec_err(errstr, "Internal error: ns_hostname is NULL");
	return NOTOK;
    }

    if (! sasl_initialized) {
	retval = sasl_client_init(NULL);
	if (retval != SASL_OK) {
	    netsec_err(errstr, "SASL client initialization failed: %s",
		       sasl_errstring(retval, NULL, NULL));
	    return NOTOK;
	}
	sasl_initialized = true;
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

    retval = sasl_client_new(service, nsc->ns_hostname, NULL, NULL,
			     nsc->sasl_cbs, 0, &nsc->sasl_conn);

    if (retval) {
	netsec_err(errstr, "SASL new client allocation failed: %s",
		   sasl_errstring(retval, NULL, NULL));
	return NOTOK;
    }

    /*
     * Set up our credentials
     */

    nsc->sasl_creds = nmh_get_credentials(nsc->ns_hostname, nsc->ns_userid);

#else /* CYRUS_SASL */
    NMH_UNUSED(service);
    NMH_UNUSED(errstr);
#endif /* CYRUS_SASL */

    /*
     * According to the RFC, mechanisms can only be uppercase letter, numbers,
     * and a hyphen or underscore.  So make sure we uppercase any letters
     * in case the user passed in lowercase.
     */

    if (mechanism) {
	nsc->sasl_mech = mh_xstrdup(mechanism);
	to_upper(nsc->sasl_mech);
    }

    nsc->sasl_proto_cb = callback;
    nsc->sasl_proto_context = context;

    return OK;
}

#ifdef CYRUS_SASL
/*
 * Our userid callback; return the specified username to the SASL
 * library when asked.
 */

int
netsec_get_user(void *context, int id, const char **result,
		    unsigned int *len)
{
    netsec_context *nsc = (netsec_context *) context;

    if (! result || (id != SASL_CB_USER && id != SASL_CB_AUTHNAME))
	return SASL_BADPARAM;

    *result = nmh_cred_get_user(nsc->sasl_creds);

    if (len)
	*len = strlen(*result);

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
    const char *password;
    int len;

    NMH_UNUSED(conn);

    if (! psecret || id != SASL_CB_PASS)
	return SASL_BADPARAM;

    password = nmh_cred_get_password(nsc->sasl_creds);

    len = strlen(password);

    /*
     * sasl_secret_t includes 1 bytes for "data" already, so that leaves
     * us room for a terminating NUL
     */

    *psecret = malloc(sizeof(sasl_secret_t) + len);

    if (! *psecret)
	return SASL_NOMEM;

    (*psecret)->len = len;
    strcpy((char *) (*psecret)->data, password);

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
#if defined CYRUS_SASL || defined OAUTH_SUPPORT
    int rc;
#endif /* CYRUS_SASL || OAUTH_SUPPORT */

    /*
     * Output some SASL information if snoop is turned on
     */

    if (nsc->ns_snoop) {
	fprintf(stderr, "SASL mechanisms supported by server: %s\n", mechlist);

	if (nsc->sasl_mech) {
		fprintf(stderr, "User has requested SASL mechanism: %s\n",
			nsc->sasl_mech);
	} else {
		fprintf(stderr, "No SASL mech selected, will pick "
			"the best mech supported by SASL library\n");
	}
    }

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

	if (nsc->ns_snoop) {
		fprintf(stderr, "Using internal XOAUTH2 mechanism\n");
	}

	if (! nsc->oauth_service) {
	    netsec_err(errstr, "Internal error: OAuth2 service name not given");
	    return NOTOK;
	}

	nsc->sasl_chosen_mech = mh_xstrdup(nsc->sasl_mech);

	if (mh_oauth_do_xoauth(nsc->ns_userid, nsc->oauth_service,
			       &xoauth_client_res, &xoauth_client_res_len,
			       nsc->ns_snoop ? stderr : NULL) != OK) {
	    netsec_err(errstr, "Internal error: Unable to get OAuth2 "
		       "bearer token");
	    return NOTOK;
	}

	rc = nsc->sasl_proto_cb(NETSEC_SASL_START, xoauth_client_res,
				xoauth_client_res_len, NULL, 0,
				nsc->sasl_proto_context, errstr);
	free(xoauth_client_res);

	if (rc != OK)
	    return NOTOK;

	/*
	 * Okay, we need to do a NETSEC_SASL_FINISH now.  If we return
	 * success, we indicate that with no output data.  But if we
	 * fail, then send a blank message and get the resulting
	 * error.
	 */

	rc = nsc->sasl_proto_cb(NETSEC_SASL_FINISH, NULL, 0, NULL, 0,
				nsc->sasl_proto_context, errstr);

	if (rc != OK) {
	    /*
	     * We're going to assume the error here is a JSON response;
	     * we ignore it and send a blank message in response.  We should
	     * then get a failure messages with a useful error.  We should
	     * NOT get a success message at this point.
	     */
	    free(*errstr);
	    nsc->sasl_proto_cb(NETSEC_SASL_WRITE, NULL, 0, NULL, 0,
			       nsc->sasl_proto_context, NULL);
	    rc = nsc->sasl_proto_cb(NETSEC_SASL_FINISH, NULL, 0, NULL, 0,
				    nsc->sasl_proto_context, errstr);
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

    if (nsc->ns_snoop) {
	const char *client_mechlist;

	rc = sasl_listmech(nsc->sasl_conn, NULL, NULL, " ", NULL,
			   &client_mechlist, NULL, NULL);

	if (rc != SASL_OK) {
		fprintf(stderr, "Unable to get client mechlist: %s\n",
			sasl_errstring(rc, NULL, NULL));
	} else {
		fprintf(stderr, "Client supported SASL mechanisms: %s\n",
			client_mechlist);
	}
    }

    ZERO(&secprops);
    secprops.maxbufsize = SASL_MAXRECVBUF;

    /*
     * If we're using TLS, do not negotiate a security layer
     */

    secprops.max_ssf = 
#ifdef TLS_SUPPORT
		nsc->tls_active ? 0 :
#endif /* TLS_SUPPORT */
		UINT_MAX;

#ifdef TLS_SUPPORT
    if (nsc->ns_snoop && nsc->tls_active)
	fprintf(stderr, "SASL security layers disabled due to the use "
		"of TLS\n");
#endif /* TLS_SUPPORT */

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

    if (nsc->ns_snoop)
	fprintf(stderr, "Chosen sasl mechanism: %s\n", chosen_mech);

    if (nsc->sasl_proto_cb(NETSEC_SASL_START, saslbuf, saslbuflen, NULL, 0,
			   nsc->sasl_proto_context, errstr) != OK)
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
			       nsc->sasl_proto_context, errstr) != OK) {
	    nsc->sasl_proto_cb(NETSEC_SASL_CANCEL, NULL, 0, NULL, 0,
			       nsc->sasl_proto_context, NULL);
	    return NOTOK;
	}

	rc = sasl_client_step(nsc->sasl_conn, (char *) outbuf, outbuflen, NULL,
			      (const char **) &saslbuf, &saslbuflen);

        free(outbuf);

	if (rc != SASL_OK && rc != SASL_CONTINUE) {
	    netsec_err(errstr, "SASL client negotiation failed: %s",
		       sasl_errdetail(nsc->sasl_conn));
	    nsc->sasl_proto_cb(NETSEC_SASL_CANCEL, NULL, 0, NULL, 0,
			       nsc->sasl_proto_context, NULL);
	    return NOTOK;
	}

	if (nsc->sasl_proto_cb(NETSEC_SASL_WRITE, saslbuf, saslbuflen,
			       NULL, 0, nsc->sasl_proto_context,
			       errstr) != OK) {
	    nsc->sasl_proto_cb(NETSEC_SASL_CANCEL, NULL, 0, NULL, 0,
			       nsc->sasl_proto_context, NULL);
	    return NOTOK;
	}
    }

    /*
     * SASL exchanges should be complete, process the final response message
     * from the server.
     */

    if (nsc->sasl_proto_cb(NETSEC_SASL_FINISH, NULL, 0, NULL, 0,
			   nsc->sasl_proto_context, errstr) != OK) {
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
	if (nsc->ns_snoop)
	    fprintf(stderr, "SASL security layer negotiated, SASL will "
		    "perform encryption\n");

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
    } else if (nsc->ns_snoop) {
	fprintf(stderr, "SASL Security layer NOT negotiated, SASL will NOT "
		"perform encryption\n");
#ifdef TLS_SUPPORT
	if (nsc->tls_active) {
	    fprintf(stderr, "Encryption will be handled by TLS\n");
	} else
#endif /* TLS_SUPPORT */
	    fprintf(stderr, "Connection will NOT be encrypted, use caution\n");
    }

    return OK;
#else
    /*
     * If we're at this point, then either we have NEITHER OAuth2 or
     * Cyrus-SASL compiled in, or have OAuth2 but didn't give the XOAUTH2
     * mechanism on the command line.
     */

    if (! nsc->sasl_mech)
	netsec_err(errstr, "SASL library support not available; please "
		   "specify a SASL mechanism to use");
    else
	netsec_err(errstr, "No support for the %s SASL mechanism",
		   nsc->sasl_mech);

    return NOTOK;
#endif /* CYRUS_SASL */
}

/*
 * Retrieve our chosen SASL mechanism
 */

char *
netsec_get_sasl_mechanism(netsec_context *nsc)
{
    return nsc->sasl_chosen_mech;
}

/*
 * Return the negotiated SASL strength security factor (SSF)
 */

int
netsec_get_sasl_ssf(netsec_context *nsc)
{
#ifdef CYRUS_SASL
    return nsc->sasl_ssf;
#else /* CYRUS_SASL */
    NMH_UNUSED(nsc);
    return 0;
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
    NMH_UNUSED(nsc);
    NMH_UNUSED(service);
    return NOTOK;
#endif /* OAUTH_SUPPORT */
}

/*
 * Initialize (and enable) TLS for this connection
 */

int
netsec_set_tls(netsec_context *nsc, int tls, int noverify, char **errstr)
{
#ifdef TLS_SUPPORT
    if (tls) {
	SSL *ssl;
        BIO *rbio, *wbio, *ssl_bio;

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

	    if (!SSL_CTX_set_default_verify_paths(sslctx)) {
		netsec_err(errstr, "Unable to set default certificate "
			   "verification paths: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return NOTOK;
	    }

	    tls_initialized = true;
	}

	if (nsc->ns_readfd == -1 || nsc->ns_writefd == -1) {
	    netsec_err(errstr, "Invalid file descriptor in netsec context");
	    return NOTOK;
	}

	if (!nsc->ns_hostname) {
	    netsec_err(errstr, "Internal error: hostname not set");
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
	 * to it.  This is done so our code stays simple if we want to use
	 * any buffering BIOs (right now we do our own buffering).
	 * So the chain looks like:
	 *
	 * SSL BIO -> socket BIO.
	 */

	rbio = BIO_new_socket(nsc->ns_readfd, BIO_CLOSE);

	if (! rbio) {
	    netsec_err(errstr, "Unable to create a read socket BIO: %s",
		       ERR_error_string(ERR_get_error(), NULL));
	    SSL_free(ssl);
	    return NOTOK;
	}

	wbio = BIO_new_socket(nsc->ns_writefd, BIO_CLOSE);

	if (! wbio) {
	    netsec_err(errstr, "Unable to create a write socket BIO: %s",
		       ERR_error_string(ERR_get_error(), NULL));
	    SSL_free(ssl);
	    BIO_free(rbio);
	    return NOTOK;
	}

	SSL_set_bio(ssl, rbio, wbio);
	SSL_set_connect_state(ssl);

	/*
	 * Use the hostname to set the Server Name Indicator extension
	 */

	SSL_set_tlsext_host_name(ssl, nsc->ns_hostname);

	/*
	 * If noverify is NOT set, then do certificate validation.
	 * Turning on SSL_VERIFY_PEER will verify the certificate chain
	 * against locally stored root certificates (the locations are
	 * set using SSL_CTX_set_default_verify_paths()), and we put
	 * the hostname in the X509 verification parameters so the OpenSSL
	 * code will verify that the hostname appears in the server
	 * certificate.
	 */

	if (! noverify) {
#ifdef HAVE_X509_VERIFY_PARAM_SET1_HOST
	    X509_VERIFY_PARAM *param;
#endif /* HAVE_X509_VERIFY_PARAM_SET1_HOST */

	    SSL_set_verify(ssl, SSL_VERIFY_PEER, NULL);

#ifdef HAVE_X509_VERIFY_PARAM_SET1_HOST
	    param = SSL_get0_param(ssl);

	    if (! X509_VERIFY_PARAM_set1_host(param, nsc->ns_hostname, 0)) {
		netsec_err(errstr, "Unable to add hostname %s to cert "
			   "verification parameters: %s", nsc->ns_hostname,
			   ERR_error_string(ERR_get_error(), NULL));
		SSL_free(ssl);
		return NOTOK;
	    }
#endif /* HAVE_X509_VERIFY_PARAM_SET1_HOST */
	}

	ssl_bio = BIO_new(BIO_f_ssl());

	if (! ssl_bio) {
	    netsec_err(errstr, "Unable to create a SSL BIO: %s",
		       ERR_error_string(ERR_get_error(), NULL));
	    SSL_free(ssl);
	    return NOTOK;
	}

	BIO_set_ssl(ssl_bio, ssl, BIO_CLOSE);
	nsc->ssl_io = ssl_bio;

	/*
	 * Since SSL now owns these file descriptors, have it handle the
	 * closing of them instead of netsec_shutdown().
	 */

	nsc->ns_noclose = 1;

	return OK;
    }
    BIO_free_all(nsc->ssl_io);
    nsc->ssl_io = NULL;

#else /* TLS_SUPPORT */
    NMH_UNUSED(nsc);
    NMH_UNUSED(noverify);

    if (tls) {
	netsec_err(errstr, "TLS is not supported");
	return NOTOK;
    }
#endif /* TLS_SUPPORT */

    return OK;
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
	unsigned long errcode = ERR_get_error();

	/*
	 * Print a more detailed message if it was certificate verification
	 * failure.
	 */

	if (ERR_GET_LIB(errcode) == ERR_LIB_SSL &&
		ERR_GET_REASON(errcode) == SSL_R_CERTIFICATE_VERIFY_FAILED) {
	    SSL *ssl;

	    if (BIO_get_ssl(nsc->ssl_io, &ssl) < 1) {
		netsec_err(errstr, "Certificate verification failed, but "
			   "cannot retrieve SSL handle: %s",
                           ERR_error_string(ERR_get_error(), NULL));
	    } else {
		netsec_err(errstr, "Server certificate verification failed: %s",
			   X509_verify_cert_error_string(
						SSL_get_verify_result(ssl)));
	    }
	} else {
	    netsec_err(errstr, "TLS negotiation failed: %s",
                       ERR_error_string(errcode, NULL));
	}

	/*
	 * Because negotiation failed, shut down TLS so we don't get any
	 * garbage on the connection.  Because of weirdness with SSL_shutdown,
	 * we end up calling it twice: once explicitly, once as part of
	 * BIO_free_all().
	 */

	BIO_ssl_shutdown(nsc->ssl_io);
	BIO_free_all(nsc->ssl_io);
	nsc->ssl_io = NULL;

	return NOTOK;
    }

    if (nsc->ns_snoop) {
	SSL *ssl;

	if (BIO_get_ssl(nsc->ssl_io, &ssl) < 1) {
	    fprintf(stderr, "WARNING: cannot determine SSL ciphers\n");
	} else {
	    const SSL_CIPHER *cipher = SSL_get_current_cipher(ssl);
	    fprintf(stderr, "TLS negotiation successful: %s(%d) %s\n",
		    SSL_CIPHER_get_name(cipher),
		    SSL_CIPHER_get_bits(cipher, NULL),
		    SSL_CIPHER_get_version(cipher));
	    SSL_SESSION_print_fp(stderr, SSL_get_session(ssl));
	}
    }

    nsc->tls_active = 1;

    return OK;
#else /* TLS_SUPPORT */
    NMH_UNUSED(nsc);
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
