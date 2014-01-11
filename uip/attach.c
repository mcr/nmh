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

static int get_line(FILE *, char *, size_t);
#ifdef MIMETYPEPROC
static char *get_file_info(const char *, const char *);
#endif /* MIMETYPEPROC */

int
attach(char *attachment_header_field_name, char *draft_file_name,
       char *body_file_name, size_t body_file_name_len,
       char *composition_file_name, size_t composition_file_name_len,
       int attachformat)
{
    char        buf[PATH_MAX + 6];      /* miscellaneous buffer */
    int         c;                      /* current character for body copy */
    int         has_attachment;         /* draft has at least one attachment */
    int         has_body;               /* draft has a message body */
    int         length;                 /* of attachment header field name */
    char        *p;                     /* miscellaneous string pointer */
    struct stat st;                     /* file status buffer */
    FILE        *fp;                    /* pointer for mhn.defaults */
    FILE        *body_file = NULL;      /* body file pointer */
    FILE        *draft_file;            /* draft file pointer */
    int         field_size;             /* size of header field buffer */
    char        *field;                 /* header field buffer */
    FILE        *composition_file;      /* composition file pointer */
    char        *build_directive;       /* mhbuild directive */


    /*
     *	Open up the draft file.
     */

    if ((draft_file = fopen(draft_file_name, "r")) == (FILE *)0)
	adios(NULL, "can't open draft file `%s'.", draft_file_name);

    /*
     *  Allocate a buffer to hold the header components as they're read in.
     *  This buffer might need to be quite large, so we grow it as needed.
     */

    field = (char *)mh_xmalloc(field_size = 256);

    /*
     *	Scan the draft file for a header field name, with a non-empty
     *	body, that matches the -attach argument.  The existence of one
     *	indicates that the draft has attachments.  Bail out if there
     *	are no attachments because we're done.  Read to the end of the
     *	headers even if we have no attachments.
     */

    length = strlen(attachment_header_field_name);

    has_attachment = 0;

    while (get_line(draft_file, field, field_size) != EOF && *field != '\0' &&
           *field != '-') {
	if (strncasecmp(field, attachment_header_field_name, length) == 0 &&
	    field[length] == ':') {
	    for (p = field + length + 1; *p == ' ' || *p == '\t'; p++)
		;
	    if (strlen (p) > 0) {
		has_attachment = 1;
	    }
	}
    }

    if (has_attachment == 0)
	return (DONE);

    /*
     *	We have at least one attachment.  Look for at least one non-blank line
     *	in the body of the message which indicates content in the body.
     */

    has_body = 0;

    while (get_line(draft_file, field, field_size) != EOF) {
	for (p = field; *p != '\0'; p++) {
	    if (*p != ' ' && *p != '\t') {
		has_body = 1;
		break;
	    }
	}

	if (has_body)
	    break;
    }

    /*
     *	Make names for the temporary files.
     */

    (void)strncpy(body_file_name,
                  m_mktemp(m_maildir(invo_name), NULL, NULL),
                  body_file_name_len);
    (void)strncpy(composition_file_name,
                  m_mktemp(m_maildir(invo_name), NULL, NULL),
                  composition_file_name_len);

    if (has_body)
	body_file = fopen(body_file_name, "w");

    composition_file = fopen(composition_file_name, "w");

    if ((has_body && body_file == (FILE *)0) || composition_file == (FILE *)0) {
	clean_up_temporary_files(body_file_name, composition_file_name);
	adios(NULL, "unable to open all of the temporary files.");
    }

    /*
     *	Start at the beginning of the draft file.  Copy all
     *	non-attachment header fields to the temporary composition
     *	file.  Then add the dashed line separator.
     */

    rewind(draft_file);

    while (get_line(draft_file, field, field_size) != EOF && *field != '\0' &&
           *field != '-')
	if (strncasecmp(field, attachment_header_field_name, length) != 0 ||
            field[length] != ':')
	    (void)fprintf(composition_file, "%s\n", field);

    (void)fputs("--------\n", composition_file);

    /*
     *	Copy the message body to a temporary file.
     */

    if (has_body) {
	while ((c = getc(draft_file)) != EOF)
		putc(c, body_file);

	(void)fclose(body_file);
    }

    /*
     *	Add a mhbuild MIME composition file line for the body if there was one.
     *  Set the default content type to text/plain so that mhbuild takes care
     *  of any necessary encoding.
     */

    if (has_body)
        /*
         * Make sure that the attachment file exists and is readable.
         */
        if (stat(body_file_name, &st) != OK  ||
            access(body_file_name, R_OK) != OK) {
            advise(NULL, "unable to access file \"%s\"", body_file_name);
            return NOTOK;
        }

    if ((build_directive = construct_build_directive (body_file_name,
                                                      "text/plain",
                                                      attachformat)) == NULL) {
        clean_up_temporary_files(body_file_name, composition_file_name);
        adios (NULL, "exiting due to failure in attach()");
    } else {
        (void) fputs(build_directive, composition_file);
        free(build_directive);
    }

    /*
     *	Now, go back to the beginning of the draft file and look for
     *	header fields that specify attachments.  Add a mhbuild MIME
     *	composition file for each.
     */

    if ((fp = fopen (p = etcpath ("mhn.defaults"), "r"))) {
	readconfig ((struct node **) NULL, fp, p, 0);
	fclose(fp);
    }

    rewind(draft_file);

    while (get_line(draft_file, field, field_size) != EOF && *field != '\0' &&
           *field != '-') {
	if (strncasecmp(field, attachment_header_field_name, length) == 0 &&
            field[length] == ':') {
	    for (p = field + length + 1; *p == ' ' || *p == '\t'; p++)
		;

	    /* Skip empty attachment_header_field_name lines. */
	    if (strlen (p) > 0) {
		struct stat st;
		if (stat(p, &st) == OK  &&  access(p, R_OK) == OK) {
		    if (S_ISREG (st.st_mode)) {
		      /* Don't set the default content type so that
			 construct_build_directive() will try to infer
			 it from the file type. */
                        if ((build_directive = construct_build_directive (p, 0,
                               attachformat)) == NULL) {
                            clean_up_temporary_files(body_file_name,
                                                     composition_file_name);
                            adios (NULL, "exiting due to failure in attach()");
                        } else {
                            (void) fputs(build_directive, composition_file);
                            free(build_directive);
                        }
		    } else {
			adios (NULL, "unable to attach %s, not a plain file",
			       p);
		    }
		} else {
		  adios (NULL, "unable to access file \"%s\"", p);
		}
	    }
	}
    }

    (void)fclose(composition_file);

    /*
     *	We're ready to roll!  Run mhbuild on the composition file.
     *	Note that mhbuild is in the context as buildmimeproc.
     */

    (void)sprintf(buf, "%s %s", buildmimeproc, composition_file_name);

    if (system(buf) != 0) {
	clean_up_temporary_files(body_file_name, composition_file_name);
	return NOTOK;
    }

    return OK;
}

void
clean_up_temporary_files(const char *body_file_name,
                         const char *composition_file_name)
{
    (void) unlink(body_file_name);
    (void) unlink(composition_file_name);

    return;
}

static int
get_line(FILE *draft_file, char *field, size_t field_size)
{
    int		c;	/* current character */
    size_t	n;	/* number of bytes in buffer */
    char	*p;	/* buffer pointer */

    /*
     * Get a line from the input file, growing the field buffer as
     * needed.  We do this so that we can fit an entire line in the
     * buffer making it easy to do a string comparison on both the
     * field name and the field body which might be a long path name.
     */

    for (n = 0, p = field; (c = getc(draft_file)) != EOF; *p++ = c) {
	if (c == '\n' && (c = getc(draft_file)) != ' ' && c != '\t') {
	    (void)ungetc(c, draft_file);
	    c = '\n';
	    break;
	}

	if (++n >= field_size - 1) {
	    field = (char *)mh_xrealloc((void *)field, field_size += 256);

	    p = field + n - 1;
	}
    }

    /*
     *	NUL-terminate the field..
     */

    *p = '\0';

    return (c);
}

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
    char buf[BUFSIZ >= 2048  ?  BUFSIZ  : 2048];
    char *cmd, *cp;
    char *quotec = "'";
    FILE *fp;

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
        if ((fp = popen(cmd, "r")) != NULL) {
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
            } else {
                advise(NULL, "unable to read mime type");
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


/*
 * Construct an mhbuild directive for the draft file.  This starts
 * with the content type.  Append a file name attribute, and depending
 * on attachformat value a private x-unix-mode attribute and a
 * description obtained (if possible) by running the "file" command on
 * the file.  Caller is responsible for free'ing returned memory.
 */
char *
construct_build_directive (char *file_name, const char *default_content_type,
                           int attachformat) {
    char *build_directive = NULL;  /* Return value. */
    char *content_type;            /* mime content type */
    char  cmd[PATH_MAX + 8];       /* file command buffer */
    struct stat st;                /* file status buffer */
    char *p;                       /* miscellaneous temporary variables */
    FILE *fp;
    int   c;                       /* current character */

    if ((content_type = mime_type (file_name)) == NULL) {
        /*
         * Check the file name for a suffix.  Scan the context for
         * that suffix on a mhshow-suffix- entry.  We use these
         * entries to be compatible with mhnshow, and there's no
         * reason to make the user specify each suffix twice.  Context
         * entries of the form "mhshow-suffix-contenttype" in the name
         * have the suffix in the field, including the dot.
         */
        struct node *np;          /* context scan node pointer */

        if ((p = strrchr(file_name, '.')) != NULL) {
            for (np = m_defs; np; np = np->n_next) {
                if (strncasecmp(np->n_name, "mhshow-suffix-", 14) == 0 &&
                    strcasecmp(p, np->n_field ? np->n_field : "") == 0) {
                    content_type = strdup (np->n_name + 14);
                    break;
                }
            }
        }

        if (content_type == NULL  &&  default_content_type != NULL) {
            content_type = strdup (default_content_type);
        }
    }

    /*
     * No content type was found, either because there was no matching
     * entry in the context or because the file name has no suffix.
     * Open the file and check for non-ASCII characters.  Choose the
     * content type based on this check.
     */
    if (content_type == NULL) {
        int  binary; /* binary character found flag */

        if ((fp = fopen(file_name, "r")) == (FILE *)0) {
            advise(NULL, "unable to access file \"%s\"", file_name);
            return NULL;
        }

        binary = 0;

        while ((c = getc(fp)) != EOF) {
            if (c > 127 || c < 0) {
                binary = 1;
                break;
            }
        }

        (void) fclose(fp);

        content_type =
            strdup (binary ? "application/octet-stream" : "text/plain");
    }

    switch (attachformat) {
    case 0: {
        struct stat st;
        char m[4];

        /* Insert name, file mode, and Content-Id. */
        if (stat(file_name, &st) != OK  ||  access(file_name, R_OK) != OK) {
            advise(NULL, "unable to access file \"%s\"", file_name);
            return NULL;
        }

        snprintf (m, sizeof m, "%.3ho", (unsigned short)(st.st_mode & 0777));
        build_directive = concat ("#", content_type, "; name=\"",
                                  ((p = strrchr(file_name, '/')) == NULL)
                                  ?  file_name
                                  :  p + 1,
                                  "\"; x-unix-mode=0", m, NULL);

        if (strlen(file_name) > PATH_MAX) {
            advise(NULL, "attachment file name `%s' too long.", file_name);
            return NULL;
        }

        (void) sprintf(cmd, "file '%s'", file_name);

        if ((fp = popen(cmd, "r")) != NULL  &&
            fgets(cmd, sizeof (cmd), fp) != NULL) {
            *strchr(cmd, '\n') = '\0';

            /*
             *  The output of the "file" command is of the form
             *
             *          file: description
             *
             *  Strip off the "file:" and subsequent white space.
             */
            for (p = cmd; *p != '\0'; p++) {
                if (*p == ':') {
                    for (p++; *p != '\0'; p++) {
                        if (*p != '\t')
                            break;
                    }
                    break;
                }
            }

            if (*p != '\0') {
                /* Insert Content-Description. */
                build_directive =
                    concat (build_directive, " [ ", p, " ]", NULL);
            }

            (void) pclose(fp);
        }
        break;
    }
    case 1:
        if (stringdex (m_maildir(invo_name), file_name) == 0) {
            /* Content had been placed by send into a temp file.
               Don't generate Content-Disposition header, because
               it confuses Microsoft Outlook, Build 10.0.6626, at
               least. */
            build_directive = concat ("#", content_type, " <>", NULL);
        } else {
            /* Suppress Content-Id, insert simple Content-Disposition
               and Content-Description with filename.
               The Content-Disposition type needs to be "inline" for
               MS Outlook and BlackBerry calendar programs to properly
               handle a text/calendar attachment. */
            p = strrchr(file_name, '/');
            build_directive = concat ("#", content_type, "; name=\"",
                                      (p == NULL) ? file_name : p + 1,
                                      "\" <> [",
                                      (p == NULL) ? file_name : p + 1,
                                      "]{",
                                      strcmp ("text/calendar", content_type)
                                        ? "attachment" : "inline",
                                      "}", NULL);
        }

        break;
    case 2:
        if (stringdex (m_maildir(invo_name), file_name) == 0) {
            /* Content had been placed by send into a temp file.
               Don't generate Content-Disposition header, because
               it confuses Microsoft Outlook, Build 10.0.6626, at
               least. */
            build_directive = concat ("#", content_type, " <>", NULL);
        } else {
            /* Suppress Content-Id, insert Content-Disposition with
               modification date and Content-Description wtih filename.
               The Content-Disposition type needs to be "inline" for
               MS Outlook and BlackBerry calendar programs to properly
               handle a text/calendar attachment. */

            if (stat(file_name, &st) != OK  ||  access(file_name, R_OK) != OK) {
                advise(NULL, "unable to access file \"%s\"", file_name);
                return NULL;
            }

            p = strrchr(file_name, '/');
            build_directive = concat ("#", content_type, "; name=\"",
                                      (p == NULL) ? file_name : p + 1,
                                      "\" <> [",
                                      (p == NULL) ? file_name : p + 1,
                                      "]{",
                                      strcmp ("text/calendar", content_type)
                                        ? "attachment" : "inline",
                                      "; modification-date=\"",
                                      dtime (&st.st_mtime, 0),
                                      "}", NULL);
        }

        break;
    default:
        advise (NULL, "unsupported attachformat %d", attachformat);
    }

    free(content_type);

    /*
     * Finish up with the file name.
     */
    build_directive = concat (build_directive, " ", file_name, "\n", NULL);

    return build_directive;
}
