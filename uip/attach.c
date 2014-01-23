/*
 * attach.c -- routines to help attach files via whatnow
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <h/tws.h>

#ifdef MIMETYPEPROC
static char *get_file_info(const char *, const char *);
#endif /* MIMETYPEPROC */

/*
 * Try to use external command to determine mime type, and possibly
 * encoding.  Caller is responsible for free'ing returned memory.
 */
char *
mime_type(const char *file_name) {
    char *content_type = NULL;  /* mime content type */

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
            } else {
                content_type = strdup(mimetype);
            }
        } else {
            content_type = strdup(mimetype);
        }
#else  /* MIMEENCODINGPROC */
        content_type = strdup(mimetype);
#endif /* MIMEENCODINGPROC */
    }
#else  /* MIMETYPEPROC */
    NMH_UNUSED(file_name);
#endif /* MIMETYPEPROC */

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

    if ((cp = strchr(file_name, '\''))) {
        /* file_name contains a single quote. */
        if (strchr(file_name, '"')) {
            advise(NULL, "filenames containing both single and double quotes "
                   "are unsupported for attachment");
            return NULL;
        } else {
            quotec = "\"";
        }
    }

    cmd = concat(proc, " ", quotec, file_name, quotec, NULL);
    if ((cmd = concat(proc, " ", quotec, file_name, quotec, NULL))) {
        FILE *fp;

        if ((fp = popen(cmd, "r")) != NULL) {
            char buf[BUFSIZ >= 2048  ?  BUFSIZ  : 2048];

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
            advise(NULL, "no output from %s", cmd);
        }

        free(cmd);
    } else {
        advise(NULL, "concat with \"%s\" failed, out of memory?", proc);
    }

    return cp  ?  strdup(cp)  :  NULL;
}
#endif /* MIMETYPEPROC */
