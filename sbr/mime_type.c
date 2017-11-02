/* mime_type.c -- routine to determine the MIME Content-Type of a file
 *
 * This code is Copyright (c) 2014, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include "error.h"
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
mime_type(const char *file_name)
{
    char *content_type = NULL;  /* mime content type */
    char *p;

#ifdef MIMETYPEPROC
    char *mimetype;

    if ((mimetype = get_file_info(MIMETYPEPROC, file_name))) {
#ifdef MIMEENCODINGPROC
        /* Try to append charset for text content. */
        char *mimeencoding;

        if (!strncasecmp(mimetype, "text", 4) &&
            (mimeencoding = get_file_info(MIMEENCODINGPROC, file_name))) {
            content_type = concat(mimetype, "; charset=", mimeencoding, NULL);
            free(mimeencoding);
            free(mimetype);
        } else
            content_type = mimetype;
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

        static bool loaded_defaults;
	if (! loaded_defaults &&
			(fp = fopen(p = etcpath("mhn.defaults"), "r"))) {
	    loaded_defaults = true;
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
            int c;

	    if (!(fp = fopen(file_name, "r"))) {
		inform("unable to access file \"%s\"", file_name);
		return NULL;
	    }

	    bool binary = false;
	    while ((c = getc(fp)) != EOF) {
		if (! isascii(c)  ||  c == 0) {
		    binary = true;
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
 * Non-null return value must be free(3)'d. */
static char *
get_file_info(const char *proc, const char *file_name)
{
    char *quotec;
    char *cmd;
    FILE *fp;
    bool ok;
    char buf[max(BUFSIZ, 2048)];
    char *info;
    char *needle;

    if (strchr(file_name, '\'')) {
        if (strchr(file_name, '"')) {
            inform("filenames containing both single and double quotes "
                "are unsupported for attachment");
            return NULL;
        }
        quotec = "\"";
    } else
        quotec = "'";

    cmd = concat(proc, " ", quotec, file_name, quotec, NULL);
    if (!cmd) {
        inform("concat with \"%s\" failed, out of memory?", proc);
        return NULL;
    }

    if ((fp = popen(cmd, "r")) == NULL) {
        inform("no output from %s", cmd);
        free(cmd);
        return NULL;
    }

    ok = fgets(buf, sizeof buf, fp);
    free(cmd);
    (void)pclose(fp);
    if (!ok)
        return NULL;

    /* s#^[^:]*:[ \t]*##. */
    info = buf;
    if ((needle = strchr(info, ':'))) {
        info = needle + 1;
        while (isblank((unsigned char)*info))
            info++;
    }

    /* s#[\n\r].*##. */
    if ((needle = strpbrk(info, "\n\r")))
        *needle = '\0';

    return strdup(info);
}
#endif /* MIMETYPEPROC */
