/* mime_type.c -- routine to determine the MIME Content-Type of a file
 *
 * This code is Copyright (c) 2014, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <h/tws.h>
#include "mime_type.h"

#ifdef MIMETYPEPROC
static char *get_file_info(const char *, const char *);
#endif /* MIMETYPEPROC */

/*
 * Try to use external command to determine mime type, and possibly
 * encoding.  If that fails try using the filename extension.  Caller
 * is responsible for free'ing returned memory.
 */
char *
mime_type(const char *file_name) {
    char *content_type = NULL;  /* mime content type */
    char *p;
    static int loaded_defaults = 0;

#ifdef MIMETYPEPROC
    char *mimetype;

    if ((mimetype = get_file_info(MIMETYPEPROC, file_name))) {
#ifdef MIMEENCODINGPROC
        /* Try to append charset for text content. */
        char *mimeencoding;

        if (strncasecmp(mimetype, "text", 4) == 0) {
            if ((mimeencoding = get_file_info(MIMEENCODINGPROC, file_name))) {
                content_type = concat(mimetype, "; charset=", mimeencoding,
                                      NULL);
                free (mimetype);
            } else {
                content_type = mimetype;
            }
        } else {
            content_type = mimetype;
        }
#else  /* MIMEENCODINGPROC */
        content_type = mimetype;
#endif /* MIMEENCODINGPROC */
    }
#endif /* MIMETYPEPROC */

    /*
     * If we didn't get the MIME type from the contents (or we don't support
     * the necessary command) then use the mhshow suffix.
     */

    if (content_type == NULL) {
	struct node *np;		/* Content scan node pointer */
	FILE *fp;			/* File pointer for mhn.defaults */

	if (! loaded_defaults &&
			(fp = fopen(p = etcpath("mhn.defaults"), "r"))) {
	    loaded_defaults = 1;
	    readconfig(NULL, fp, p, 0);
	    fclose(fp);
	}

	if ((p = strrchr(file_name, '.')) != NULL) {
	    for (np = m_defs; np; np = np->n_next) {
		if (strncasecmp(np->n_name, "mhshow-suffix-", 14) == 0 &&
		    strcasecmp(p, FENDNULL(np->n_field)) == 0) {
		    content_type = strdup(np->n_name + 14);
		    break;
		}
	    }
	}

	/*
	 * If we didn't match any filename extension, try to infer the
	 * content type. If we have binary, assume application/octet-stream;
	 * otherwise, assume text/plain.
	 */

	if (content_type == NULL) {
	    FILE *fp;
	    int binary = 0, c;

	    if (!(fp = fopen(file_name, "r"))) {
		inform("unable to access file \"%s\"", file_name);
		return NULL;
	    }

	    while ((c = getc(fp)) != EOF) {
		if (! isascii(c)  ||  c == 0) {
		    binary = 1;
		    break;
		}
	    }

	    fclose(fp);

	    content_type =
		strdup(binary ? "application/octet-stream" : "text/plain");
	}
    }

    return content_type;
}


#ifdef MIMETYPEPROC
/*
 * Get information using proc about a file.
 */
static char *
get_file_info(const char *proc, const char *file_name) {
    char *cmd, *cp;
    char *quotec = "'";
    char buf[max(BUFSIZ, 2048)];

    if ((cp = strchr(file_name, '\''))) {
        /* file_name contains a single quote. */
        if (strchr(file_name, '"')) {
            inform("filenames containing both single and double quotes "
                   "are unsupported for attachment");
            return NULL;
        }
        quotec = "\"";
    }

    cp = NULL;
    if ((cmd = concat(proc, " ", quotec, file_name, quotec, NULL))) {
        FILE *fp;

        if ((fp = popen(cmd, "r")) != NULL) {

            buf[0] = '\0';
            if (fgets(buf, sizeof buf, fp)) {
                char *eol;

                /* Skip leading <filename>:<whitespace>, if present. */
                if ((cp = strchr(buf, ':')) != NULL) {
                    ++cp;
                    while (*cp  &&  isblank((unsigned char) *cp)) {
                        ++cp;
                    }
                } else {
                    cp = buf;
                }

                /* Truncate at newline (LF or CR), if present. */
                if ((eol = strpbrk(cp, "\n\r")) != NULL) {
                    *eol = '\0';
                }
            } else if (buf[0] == '\0') {
                /* This can happen on Cygwin if the popen()
                   mysteriously fails.  Return NULL so that the caller
                   will use another method to determine the info. */
                free (cp);
                cp = NULL;
            }

            (void) pclose(fp);
        } else {
            inform("no output from %s", cmd);
        }

        free(cmd);
    } else {
        inform("concat with \"%s\" failed, out of memory?", proc);
    }

    return cp  ?  strdup(cp)  :  NULL;
}
#endif /* MIMETYPEPROC */
