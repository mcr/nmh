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

static int   get_line(char *, size_t);
static int   make_mime_composition_file_entry(char *, int, char *);

static	FILE	*draft_file;				/* draft file pointer */
static	FILE	*composition_file;			/* composition file pointer */

int
attach(char *attachment_header_field_name, char *draft_file_name,
       char *body_file_name, size_t body_file_name_len,
       char *composition_file_name, size_t composition_file_name_len,
       int attachformat)
{
    char		buf[PATH_MAX + 6];	/* miscellaneous buffer */
    int			c;			/* current character for body copy */
    int			has_attachment;		/* draft has at least one attachment */
    int			has_body;		/* draft has a message body */
    int			length;			/* length of attachment header field name */
    char		*p;			/* miscellaneous string pointer */
    FILE		*fp;			/* pointer for mhn.defaults */
    FILE		*body_file = NULL;	/* body file pointer */
    int			field_size;		/* size of header field buffer */
    char		*field;			/* header field buffer */

    /*
     *	Open up the draft file.
     */

    if ((draft_file = fopen(draft_file_name, "r")) == (FILE *)0)
	adios((char *)0, "can't open draft file `%s'.", draft_file_name);

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

    while (get_line(field, field_size) != EOF && *field != '\0' &&
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

    while (get_line(field, field_size) != EOF) {
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
	adios((char *)0, "unable to open all of the temporary files.");
    }

    /*
     *	Start at the beginning of the draft file.  Copy all
     *	non-attachment header fields to the temporary composition
     *	file.  Then add the dashed line separator.
     */

    rewind(draft_file);

    while (get_line(field, field_size) != EOF && *field != '\0' &&
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
	if (make_mime_composition_file_entry(body_file_name, attachformat,
					     "text/plain")) {
	    clean_up_temporary_files(body_file_name, composition_file_name);
	    adios (NULL, "exiting");
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

    while (get_line(field, field_size) != EOF && *field != '\0' &&
           *field != '-') {
	if (strncasecmp(field, attachment_header_field_name, length) == 0 &&
            field[length] == ':') {
	    for (p = field + length + 1; *p == ' ' || *p == '\t'; p++)
		;

	    /* Skip empty attachment_header_field_name lines. */
	    if (strlen (p) > 0) {
		struct stat st;
		if (stat (p, &st) == OK) {
		    if (S_ISREG (st.st_mode)) {
		      /* Don't set the default content type so take
			 make_mime_composition_file_entry() will try
			 to infer it from the file type. */
			if (make_mime_composition_file_entry(p, attachformat, 0)) {
			    clean_up_temporary_files(body_file_name,
                                                     composition_file_name);
			    adios (NULL, "exiting");
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
    (void)unlink(body_file_name);
    (void)unlink(composition_file_name);

    return;
}

static int
get_line(char *field, size_t field_size)
{
    int		c;	/* current character */
    size_t	n;	/* number of bytes in buffer */
    char	*p;	/* buffer pointer */

    /*
     *	Get a line from the input file, growing the field buffer as needed.  We do this
     *	so that we can fit an entire line in the buffer making it easy to do a string
     *	comparison on both the field name and the field body which might be a long path
     *	name.
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

static int
make_mime_composition_file_entry(char *file_name, int attachformat,
                                 char *default_content_type)
{
    int			binary;			/* binary character found flag */
    int			c;			/* current character */
    char		cmd[PATH_MAX + 8];	/* file command buffer */
    char		*content_type;		/* mime content type */
    FILE		*fp;			/* content and pipe file pointer */
    struct	node	*np;			/* context scan node pointer */
    char		*p;			/* miscellaneous string pointer */
    struct	stat	st;			/* file status buffer */

    if ((content_type = mime_type (file_name)) == NULL) {
        /*
         * Check the file name for a suffix.  Scan the context for
         * that suffix on a mhshow-suffix- entry.  We use these
         * entries to be compatible with mhnshow, and there's no
         * reason to make the user specify each suffix twice.  Context
         * entries of the form "mhshow-suffix-contenttype" in the name
         * have the suffix in the field, including the dot.
         */
        if ((p = strrchr(file_name, '.')) != (char *)0) {
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
     *	No content type was found, either because there was no matching entry in the
     *	context or because the file name has no suffix.  Open the file and check for
     *	non-ASCII characters.  Choose the content type based on this check.
     */

    if (content_type == (char *)0) {
	if ((fp = fopen(file_name, "r")) == (FILE *)0) {
	    advise((char *)0, "unable to access file \"%s\"", file_name);
            return NOTOK;
	}

	binary = 0;

	while ((c = getc(fp)) != EOF) {
	    if (c > 127 || c < 0) {
		binary = 1;
		break;
	    }
	}

	(void)fclose(fp);

	content_type =
            strdup (binary ? "application/octet-stream" : "text/plain");
    }

    /*
     *	Make sure that the attachment file exists and is readable.  Append a mhbuild
     *	directive to the draft file.  This starts with the content type.  Append a
     *	file name attribute and a private x-unix-mode attribute.  Also append a
     *	description obtained (if possible) by running the "file" command on the file.
     */

    if (stat(file_name, &st) == -1 || access(file_name, R_OK) != 0) {
	advise((char *)0, "unable to access file \"%s\"", file_name);
        return NOTOK;
    }

    switch (attachformat) {
    case 0:
        /* Insert name, file mode, and Content-Id. */
        (void)fprintf(composition_file, "#%s; name=\"%s\"; x-unix-mode=0%.3ho",
            content_type, ((p = strrchr(file_name, '/')) == (char *)0) ? file_name : p + 1, (unsigned short)(st.st_mode & 0777));

        if (strlen(file_name) > PATH_MAX) {
            advise((char *)0, "attachment file name `%s' too long.", file_name);
            return NOTOK;
        }

        (void)sprintf(cmd, "file '%s'", file_name);

        if ((fp = popen(cmd, "r")) != (FILE *)0 && fgets(cmd, sizeof (cmd), fp) != (char *)0) {
            *strchr(cmd, '\n') = '\0';

            /*
             *  The output of the "file" command is of the form
             *
             *  	file:	description
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

            if (*p != '\0')
                /* Insert Content-Description. */
                (void)fprintf(composition_file, " [ %s ]", p);

            (void)pclose(fp);
        }

        break;
    case 1:
        if (stringdex (m_maildir(invo_name), file_name) == 0) {
            /* Content had been placed by send into a temp file.
               Don't generate Content-Disposition header, because
               it confuses Microsoft Outlook, Build 10.0.6626, at
               least. */
            (void) fprintf (composition_file, "#%s <>", content_type);
        } else {
            /* Suppress Content-Id, insert simple Content-Disposition
               and Content-Description with filename.
               The Content-Disposition type needs to be "inline" for
               MS Outlook and BlackBerry calendar programs to properly
               handle a text/calendar attachment. */
            p = strrchr(file_name, '/');
            (void) fprintf (composition_file,
                            "#%s; name=\"%s\" <> [%s]{%s}",
                            content_type,
                            (p == (char *)0) ? file_name : p + 1,
                            (p == (char *)0) ? file_name : p + 1,
                            strcmp ("text/calendar", content_type)
                              ? "attachment" : "inline");
        }

        break;
    case 2:
        if (stringdex (m_maildir(invo_name), file_name) == 0) {
            /* Content had been placed by send into a temp file.
               Don't generate Content-Disposition header, because
               it confuses Microsoft Outlook, Build 10.0.6626, at
               least. */
            (void) fprintf (composition_file, "#%s <>", content_type);
        } else {
            /* Suppress Content-Id, insert Content-Disposition with
               modification date and Content-Description wtih filename.
               The Content-Disposition type needs to be "inline" for
               MS Outlook and BlackBerry calendar programs to properly
               handle a text/calendar attachment. */
            p = strrchr(file_name, '/');
            (void) fprintf (composition_file,
                            "#%s; name=\"%s\" <>[%s]{%s; "
                            "modification-date=\"%s\"}",
                            content_type,
                            (p == (char *)0) ? file_name : p + 1,
                            (p == (char *)0) ? file_name : p + 1,
                            strcmp ("text/calendar", content_type)
                              ? "attachment" : "inline",
                            dtime (&st.st_mtime, 0));
        }

        break;
    default:
        adios ((char *)0, "unsupported attachformat %d", attachformat);
    }

    free (content_type);

    /*
     *	Finish up with the file name.
     */

    (void)fprintf(composition_file, " %s\n", file_name);

    return OK;
}

/*
 * Try to use external command to determine mime type, and possibly
 * encoding.  Caller is responsible for free'ing returned memory.
 */
char *
mime_type (const char *file_name) {
    char *content_type = NULL;  /* mime content type */

#ifdef MIMETYPEPROC
    char cmd[2 * PATH_MAX + 2];   /* file command buffer */
    char buf[BUFSIZ >= 2048  ?  BUFSIZ  : 2048];
    FILE *fp;                   /* content and pipe file pointer */
    char mimetypeproc[] = MIMETYPEPROC " '%s'";

    if ((int) snprintf (cmd, sizeof cmd, mimetypeproc, file_name) <
        (int) sizeof cmd) {
        if ((fp = popen (cmd, "r")) != NULL) {
            /* Make sure that buf has space for one additional
               character, the semicolon that might be added below. */
            if (fgets (buf, sizeof buf - 1, fp)) {
                char *cp, *space;

                /* Skip leading <filename>:<whitespace>, if present. */
                if ((content_type = strchr (buf, ':')) != NULL) {
                    ++content_type;
                    while (*content_type  &&  isblank (*content_type)) {
                        ++content_type;
                    }
                } else {
                    content_type = buf;
                }

                /* Truncate at newline (LF or CR), if present. */
                if ((cp = strpbrk (content_type, "\n\n")) != NULL) {
                    *cp = '\0';
                }

                /* If necessary, insert semicolon between content type
                   and charset.  Assume that the first space is between
                   them. */
                if ((space = strchr (content_type, ' ')) != NULL) {
                    ssize_t len = strlen (content_type);

                    if (space - content_type > 0  &&
                        len > space - content_type + 1) {
                        if (*(space - 1) != ';') {
                            /* The +1 is for the terminating NULL. */
                            memmove (space + 1, space,
                                     len - (space - content_type) + 1);
                            *space = ';';
                        }
                    }
                }
            } else {
                advise (NULL, "unable to read mime type");
            }
        } else {
            advise (NULL, "unable to run %s", buf);
        }
    } else {
        advise (NULL, "filename to large to deduce mime type");
    }
#else
    NMH_UNUSED (file_name);
#endif

    return content_type ? strdup (content_type) : NULL;
}
