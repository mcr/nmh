
/*
 * mhoutsbr.c -- routines to output MIME messages
 *            -- given a Content structure
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/signals.h>
#include <h/md5.h>
#include <errno.h>
#include <signal.h>
#include <h/mts.h>
#include <h/tws.h>
#include <h/mime.h>
#include <h/mhparse.h>


/*
 * prototypes
 */
int output_message (CT, char *);
int output_message_fp (CT, FILE *, char *);

/*
 * static prototypes
 */
static int output_content (CT, FILE *);
static void output_headers (CT, FILE *);
static int writeExternalBody (CT, FILE *);
static int write8Bit (CT, FILE *);
static int writeQuoted (CT, FILE *);
static int writeBase64ct (CT, FILE *);


/*
 * Main routine to output a MIME message contained
 * in a Content structure, to a file.  Any necessary
 * transfer encoding is added.
 */

int
output_message_fp (CT ct, FILE *fp, char *file)
{
    if (output_content (ct, fp) == NOTOK)
	return NOTOK;

    if (fflush (fp)) {
	advise ((file?file:"<FILE*>"), "error writing to");
	return NOTOK;
    }
    return OK;
}

int
output_message (CT ct, char *file)
{
    FILE *fp;
    int status;

    if (! strcmp (file, "-")) {
	fp = stdout;
    } else if ((fp = fopen (file, "w")) == NULL) {
	advise (file, "unable to open for writing");
	return NOTOK;
    }
    status = output_message_fp(ct, fp, file);
    if (strcmp (file, "-")) fclose(fp);
    return status;
}


/*
 * Output a Content structure to a file.
 */

static int
output_content (CT ct, FILE *out)
{
    int result = 0;
    CI ci = &ct->c_ctinfo;
    char *boundary = ci->ci_values[0], **ap, **vp;

    for (ap = ci->ci_attrs, vp = ci->ci_values; *ap; ++ap, ++vp) {
        if (! strcasecmp ("boundary", *ap)) {
            boundary = *vp;
            break;
        }
    }

    /*
     * Output all header fields for this content
     */
    output_headers (ct, out);

    /*
     * If this is the internal content structure for a
     * "message/external", then we are done with the
     * headers (since it has no body).
     */
    if (ct->c_ctexbody)
	return OK;

    /*
     * Now output the content bodies.
     */
    switch (ct->c_type) {
    case CT_MULTIPART:
    {
	struct multipart *m;
	struct part *part;

	if (ct->c_rfc934)
	    putc ('\n', out);

	m = (struct multipart *) ct->c_ctparams;

        if (m->mp_content_before) {
           fprintf (out, "%s", m->mp_content_before);
        }

	for (part = m->mp_parts; part; part = part->mp_next) {
	    CT p = part->mp_part;

	    fprintf (out, "\n--%s\n", boundary);
	    if (output_content (p, out) == NOTOK)
		return NOTOK;
	}
	fprintf (out, "\n--%s--\n", boundary);

        if (m->mp_content_after) {
           fprintf (out, "%s", m->mp_content_after);
        }
    }
    break;

    case CT_MESSAGE:
	putc ('\n', out);
	if (ct->c_subtype == MESSAGE_EXTERNAL) {
	    struct exbody *e;

	    e = (struct exbody *) ct->c_ctparams;
	    if (output_content (e->eb_content, out) == NOTOK)
		return NOTOK;

	    /* output phantom body for access-type "mail-server" */
	    if (e->eb_body)
		writeExternalBody (ct, out);
	} else {
	    result = write8Bit (ct, out);
	}
	break;

    /*
     * Handle discrete types (text/application/audio/image/video)
     */
    default:
	switch (ct->c_encoding) {
	case CE_7BIT:
	    /* Special case:  if this is a non-MIME message with no
	       body, don't emit the newline that would appear between
	       the headers and body.  In that case, the call to
	       write8Bit() shouldn't be needed, but is harmless. */
	    if (ct->c_ctinfo.ci_attrs[0] != NULL  ||
		ct->c_begin != ct->c_end) {
		putc ('\n', out);
	    }
	    result = write8Bit (ct, out);
	    break;

	case CE_8BIT:
	    putc ('\n', out);
	    result = write8Bit (ct, out);
	    break;

	case CE_QUOTED:
	    putc ('\n', out);
	    result = writeQuoted (ct, out);
	    break;

	case CE_BASE64:
	    putc ('\n', out);
	    result = writeBase64ct (ct, out);
	    break;

	case CE_BINARY:
	    advise (NULL, "can't handle binary transfer encoding in content");
	    result = NOTOK;
	    break;

	default:
	    advise (NULL, "unknown transfer encoding in content");
	    result = NOTOK;
	    break;
	}
	break;
    }

    return result;
}


/*
 * Output all the header fields for a content
 */

static void
output_headers (CT ct, FILE *out)
{
    HF hp;

    hp = ct->c_first_hf;
    while (hp) {
	fprintf (out, "%s:%s", hp->name, hp->value);
	hp = hp->next;
    }
}


/*
 * Write the phantom body for access-type "mail-server".
 */

static int
writeExternalBody (CT ct, FILE *out)
{
    char **ap, **ep, *cp;
    struct exbody *e = (struct exbody *) ct->c_ctparams;
		
    putc ('\n', out);
    for (cp = e->eb_body; *cp; cp++) {
	CT ct2 = e->eb_content;
	CI ci2 = &ct2->c_ctinfo;

	if (*cp == '\\') {
	    switch (*++cp) {
	    case 'I':
		if (ct2->c_id) {
		    char *dp = trimcpy (ct2->c_id);

		    fputs (dp, out);
		    free (dp);
		}
		continue;

	    case 'N':
		for (ap = ci2->ci_attrs, ep = ci2->ci_values; *ap; ap++, ep++)
		    if (!strcasecmp (*ap, "name")) {
			fprintf (out, "%s", *ep);
			break;
		    }
		continue;

	    case 'T':
		fprintf (out, "%s/%s", ci2->ci_type, ci2->ci_subtype);
		for (ap = ci2->ci_attrs, ep = ci2->ci_values; *ap; ap++, ep++)
		    fprintf (out, "; %s=\"%s\"", *ap, *ep);
		continue;

	    case 'n':
		putc ('\n', out);
		continue;

	    case 't':
		putc ('\t', out);
		continue;

	    case '\0':
		cp--;
		break;

	    case '\\':
	    case '"':
		break;

	    default:
		putc ('\\', out);
		break;
	    }
	}
	putc (*cp, out);
    }
    putc ('\n', out);

    return OK;
}


/*
 * Output a content without any transfer encoding
 */

static int
write8Bit (CT ct, FILE *out)
{
    int fd;
    size_t inbytes;
    char c, *file, buffer[BUFSIZ];
    CE ce = &ct->c_cefile;

    file = NULL;
    if ((fd = (*ct->c_ceopenfnx) (ct, &file)) == NOTOK)
	return NOTOK;

    c = '\n';
    while ((inbytes = fread (buffer, 1, sizeof buffer, ce->ce_fp)) > 0) {
        c = buffer[inbytes - 1];
        fwrite (buffer, 1, inbytes, out);
    }
    if (c != '\n')
	putc ('\n', out);

    (*ct->c_ceclosefnx) (ct);
    return OK;
}


/*
 * Output a content using quoted-printable
 */

static int
writeQuoted (CT ct, FILE *out)
{
    int fd;
    char *cp, *file;
    char c, buffer[BUFSIZ];
    CE ce = &ct->c_cefile;

    file = NULL;
    if ((fd = (*ct->c_ceopenfnx) (ct, &file)) == NOTOK)
	return NOTOK;

    while (fgets (buffer, sizeof(buffer) - 1, ce->ce_fp)) {
	int n;

	cp = buffer + strlen (buffer) - 1;
	if ((c = *cp) == '\n')
	    *cp = '\0';

	if (strncmp (cp = buffer, "From ", sizeof("From ") - 1) == 0) {
	    fprintf (out, "=%02X", *cp++ & 0xff);
	    n = 3;
	} else {
	    n = 0;
	}
	for (; *cp; cp++) {
	    if (n > CPERLIN - 3) {
		fputs ("=\n", out);
		n = 0;
	    }

	    switch (*cp) {
		case ' ':
		case '\t':
		    putc (*cp, out);
		    n++;
		    break;

		default:
		    if (*cp < '!' || *cp > '~')
			goto three_print;
		    putc (*cp, out);
		    n++;
		    break;

		case '=':
three_print:
		    fprintf (out, "=%02X", *cp & 0xff);
		    n += 3;
		    break;
	    }
	}

	if (c == '\n') {
	    if (cp > buffer && (*--cp == ' ' || *cp == '\t'))
		fputs ("=\n", out);

	    putc ('\n', out);
	} else {
	    fputs ("=\n", out);
	}
    }

    (*ct->c_ceclosefnx) (ct);
    return OK;
}


/*
 * Output a content using base64
 */

static int
writeBase64ct (CT ct, FILE *out)
{
    int	fd, result;
    char *file;
    CE ce = &ct->c_cefile;

    file = NULL;
    if ((fd = (*ct->c_ceopenfnx) (ct, &file)) == NOTOK)
	return NOTOK;

    result = writeBase64aux (ce->ce_fp, out);
    (*ct->c_ceclosefnx) (ct);
    return result;
}
