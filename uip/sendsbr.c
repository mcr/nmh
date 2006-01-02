
/*
 * sendsbr.c -- routines to help WhatNow/Send along
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/signals.h>
#include <setjmp.h>
#include <signal.h>
#include <fcntl.h>
#include <h/mime.h>
#include <h/utils.h>

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef TM_IN_SYS_TIME
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

int debugsw = 0;		/* global */
int forwsw  = 1;
int inplace = 1;
int pushsw  = 0;
int splitsw = -1;
int unique  = 0;
int verbsw  = 0;

char *altmsg   = NULL;		/*  .. */
char *annotext = NULL;
char *distfile = NULL;

static int armed = 0;
static jmp_buf env;

static	char	body_file_name[MAXPATHLEN + 1];		/* name of temporary file for body content */
static	char	composition_file_name[MAXPATHLEN + 1];	/* name of mhbuild composition temporary file */
static	int	field_size;				/* size of header field buffer */
static	char	*field;					/* header field buffer */
static	FILE	*draft_file;				/* draft file pointer */
static	FILE	*body_file;				/* body file pointer */
static	FILE	*composition_file;			/* composition file pointer */

/*
 * external prototypes
 */
int sendsbr (char **, int, char *, struct stat *, int, char *);
int done (int);
char *getusername (void);

/*
 * static prototypes
 */
static void alert (char *, int);
static int tmp_fd (void);
static void anno (int, struct stat *);
static void annoaux (int);
static int splitmsg (char **, int, char *, struct stat *, int);
static int sendaux (char **, int, char *, struct stat *);

static	int	attach(char *, char *);
static	void	clean_up_temporary_files(void);
static	int	get_line(void);
static	void	make_mime_composition_file_entry(char *);


/*
 * Entry point into (back-end) routines to send message.
 */

int
sendsbr (char **vec, int vecp, char *drft, struct stat *st, int rename_drft, char *attachment_header_field_name)
{
    int status;
    char buffer[BUFSIZ], file[BUFSIZ];
    struct stat sts;
    char	*original_draft;		/* name of original draft file */
    char	*p;				/* string pointer for building file name */

    /*
     *	Save the original name of the draft file.  The name of the draft file is changed
     *	to a temporary file containing the built MIME message if there are attachments.
     *	We need the original name so that it can be renamed after the message is sent.
     */

    original_draft = drft;

    /*
     *	There might be attachments if a header field name for attachments is supplied.
     *	Convert the draft to a MIME message.  Use the mhbuild composition file for the
     *	draft if there was a successful conversion because that now contains the MIME
     *	message.  A nice side effect of this is that it leaves the original draft file
     *	untouched so that it can be retrieved and modified if desired.
     */

    if (attachment_header_field_name != (char *)0) {
	switch (attach(attachment_header_field_name, drft)) {
	case OK:
	    drft = composition_file_name;
	    break;

	case NOTOK:
	    return (NOTOK);

	case DONE:
	    break;
	}
    }

    armed++;
    switch (setjmp (env)) {
    case OK: 
	/*
	 * If given -push and -unique (which is undocumented), then
	 * rename the draft file.  I'm not quite sure why.
	 */
	if (pushsw && unique) {
	    if (rename (drft, strncpy (file, m_scratch (drft, invo_name), sizeof(file)))
		    == NOTOK)
		adios (file, "unable to rename %s to", drft);
	    drft = file;
	}

	/*
	 * Check if we need to split the message into
	 * multiple messages of type "message/partial".
	 */
	if (splitsw >= 0 && !distfile && stat (drft, &sts) != NOTOK
		&& sts.st_size >= CPERMSG) {
	    status = splitmsg (vec, vecp, drft, st, splitsw) ? NOTOK : OK;
	} else {
	    status = sendaux (vec, vecp, drft, st) ? NOTOK : OK;
	}

	/* rename the original draft */
	if (rename_drft && status == OK &&
		rename (original_draft, strncpy (buffer, m_backup (original_draft), sizeof(buffer))) == NOTOK)
	    advise (buffer, "unable to rename %s to", drft);
	break;

    default: 
	status = DONE;
	break;
    }

    armed = 0;
    if (distfile)
	unlink (distfile);

    /*
     *	Get rid of any temporary files that we created for attachments.  Also get rid of
     *	the renamed composition file that mhbuild leaves as a turd.  It looks confusing,
     *	but we use the body file name to help build the renamed composition file name.
     */

    if (drft == composition_file_name) {
	clean_up_temporary_files();

	if (strlen(composition_file_name) >= sizeof (composition_file_name) - 6)
	    advise((char *)0, "unable to remove original composition file.");

	else {
	    if ((p = strrchr(composition_file_name, '/')) == (char *)0)
		p = composition_file_name;
	    else
		p++;
	    
	    (void)strcpy(body_file_name, p);
	    *p++ = ',';
	    (void)strcpy(p, body_file_name);
	    (void)strcat(p, ".orig");
	    
	    (void)unlink(composition_file_name);
	}
    }

    return status;
}

static	int
attach(char *attachment_header_field_name, char *draft_file_name)
{
    char		buf[MAXPATHLEN + 6];	/* miscellaneous buffer */
    int			c;			/* current character for body copy */
    int			has_attachment;		/* draft has at least one attachment */
    int			has_body;		/* draft has a message body */
    int			length;			/* length of attachment header field name */
    char		*p;			/* miscellaneous string pointer */

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
     *	Scan the draft file for a header field name that matches the -attach
     *	argument.  The existence of one indicates that the draft has attachments.
     *	Bail out if there are no attachments because we're done.  Read to the
     *	end of the headers even if we have no attachments.
     */

    length = strlen(attachment_header_field_name);

    has_attachment = 0;

    while (get_line() != EOF && *field != '\0' && *field != '-')
	if (strncasecmp(field, attachment_header_field_name, length) == 0 && field[length] == ':')
	    has_attachment = 1;

    if (has_attachment == 0)
	return (DONE);

    /*
     *	We have at least one attachment.  Look for at least one non-blank line
     *	in the body of the message which indicates content in the body.
     */

    has_body = 0;

    while (get_line() != EOF) {
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

    (void)strncpy(body_file_name, m_scratch("", m_maildir(invo_name)), sizeof (body_file_name));
    (void)strncpy(composition_file_name, m_scratch("", m_maildir(invo_name)), sizeof (composition_file_name));

    if (has_body)
	body_file = fopen(body_file_name, "w");

    composition_file = fopen(composition_file_name, "w");

    if ((has_body && body_file == (FILE *)0) || composition_file == (FILE *)0) {
	clean_up_temporary_files();
	adios((char *)0, "unable to open all of the temporary files.");
    }

    /*
     *	Start at the beginning of the draft file.  Copy all non-attachment header fields
     *	to the temporary composition file.  Then add the dashed line separator.
     */

    rewind(draft_file);

    while (get_line() != EOF && *field != '\0' && *field != '-')
	if (strncasecmp(field, attachment_header_field_name, length) != 0 || field[length] != ':')
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
     */

    if (has_body)
	make_mime_composition_file_entry(body_file_name);

    /*
     *	Now, go back to the beginning of the draft file and look for header fields
     *	that specify attachments.  Add a mhbuild MIME composition file for each.
     */

    rewind(draft_file);

    while (get_line() != EOF && *field != '\0' && *field != '-') {
	if (strncasecmp(field, attachment_header_field_name, length) == 0 && field[length] == ':') {
	    for (p = field + length + 1; *p == ' ' || *p == '\t'; p++)
		;

	    make_mime_composition_file_entry(p);
	}
    }

    (void)fclose(composition_file);

    /*
     *	We're ready to roll!  Run mhbuild on the composition file.  Note that mhbuild
     *	is in the context as buildmimeproc.
     */

    (void)sprintf(buf, "%s %s", buildmimeproc, composition_file_name);

    if (system(buf) != 0) {
	clean_up_temporary_files();
	return (NOTOK);
    }

    return (OK);
}

static	void
clean_up_temporary_files(void)
{
    (void)unlink(body_file_name);
    (void)unlink(composition_file_name);

    return;
}

static	int
get_line(void)
{
    int		c;	/* current character */
    int		n;	/* number of bytes in buffer */
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
	    if ((field = (char *)realloc((void *)field, field_size += 256)) == (char *)0)
		adios(NULL, "can't grow field buffer.");

	    p = field + n - 1;
	}
    }

    /*
     *	NUL-terminate the field..
     */

    *p = '\0';

    return (c);
}

static	void
make_mime_composition_file_entry(char *file_name)
{
    int			binary;			/* binary character found flag */
    int			c;			/* current character */
    char		cmd[MAXPATHLEN + 6];	/* file command buffer */
    char		*content_type;		/* mime content type */
    FILE		*fp;			/* content and pipe file pointer */
    struct	node	*np;			/* context scan node pointer */
    char		*p;			/* miscellaneous string pointer */
    struct	stat	st;			/* file status buffer */

    content_type = (char *)0;

    /*
     *	Check the file name for a suffix.  Scan the context for that suffix on a
     *	mhshow-suffix- entry.  We use these entries to be compatible with mhnshow,
     *	and there's no reason to make the user specify each suffix twice.  Context
     *	entries of the form "mhshow-suffix-contenttype" in the name have the suffix
     *	in the field, including the dot.
     */

    if ((p = strrchr(file_name, '.')) != (char *)0) {
	for (np = m_defs; np; np = np->n_next) {
	    if (strncasecmp(np->n_name, "mhshow-suffix-", 14) == 0 && strcasecmp(p, np->n_field) == 0) {
		content_type = np->n_name + 14;
		break;
	    }
	}
    }

    /*
     *	No content type was found, either because there was no matching entry in the
     *	context or because the file name has no suffix.  Open the file and check for
     *	non-ASCII characters.  Choose the content type based on this check.
     */

    if (content_type == (char *)0) {
	if ((fp = fopen(file_name, "r")) == (FILE *)0) {
	    clean_up_temporary_files();
	    adios((char *)0, "unable to access file \"%s\"", file_name);
	}

	binary = 0;

	while ((c = getc(fp)) != EOF) {
	    if (c > 127 || c < 0) {
		binary = 1;
		break;
	    }
	}

	(void)fclose(fp);

	content_type = binary ? "application/octet-stream" : "text/plain";
    }

    /*
     *	Make sure that the attachment file exists and is readable.  Append a mhbuild
     *	directive to the draft file.  This starts with the content type.  Append a
     *	file name attribute and a private x-unix-mode attribute.  Also append a
     *	description obtained (if possible) by running the "file" command on the file.
     */

    if (stat(file_name, &st) == -1 || access(file_name, R_OK) != 0) {
	clean_up_temporary_files();
	adios((char *)0, "unable to access file \"%s\"", file_name);
    }

    (void)fprintf(composition_file, "#%s; name=\"%s\"; x-unix-mode=0%.3ho",
     content_type, ((p = strrchr(file_name, '/')) == (char *)0) ? file_name : p + 1, (unsigned short)(st.st_mode & 0777));

    if (strlen(file_name) > MAXPATHLEN) {
	clean_up_temporary_files();
	adios((char *)0, "attachment file name `%s' too long.", file_name);
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
	    (void)fprintf(composition_file, " [ %s ]", p);

	(void)pclose(fp);
    }

    /*
     *	Finish up with the file name.
     */

    (void)fprintf(composition_file, " %s\n", file_name);

    return;
}

/*
 * Split large message into several messages of
 * type "message/partial" and send them.
 */

static int
splitmsg (char **vec, int vecp, char *drft, struct stat *st, int delay)
{
    int	compnum, nparts, partno, state, status;
    long pos, start;
    time_t clock;
    char *cp, *dp, buffer[BUFSIZ], msgid[BUFSIZ];
    char subject[BUFSIZ];
    char name[NAMESZ], partnum[BUFSIZ];
    FILE *in;

    if ((in = fopen (drft, "r")) == NULL)
	adios (drft, "unable to open for reading");

    cp = dp = NULL;
    start = 0L;

    /*
     * Scan through the message and examine the various header fields,
     * as well as locate the beginning of the message body.
     */
    for (compnum = 1, state = FLD;;) {
	switch (state = m_getfld (state, name, buffer, sizeof(buffer), in)) {
	    case FLD:
	    case FLDPLUS:
	    case FLDEOF:
	        compnum++;

		/*
		 * This header field is discarded.
		 */
		if (!strcasecmp (name, "Message-ID")) {
		    while (state == FLDPLUS)
			state = m_getfld (state, name, buffer, sizeof(buffer), in);
		} else if (uprf (name, XXX_FIELD_PRF)
			|| !strcasecmp (name, VRSN_FIELD)
			|| !strcasecmp (name, "Subject")
		        || !strcasecmp (name, "Encrypted")) {
		    /*
		     * These header fields are copied to the enclosed
		     * header of the first message in the collection
		     * of message/partials.  For the "Subject" header
		     * field, we also record it, so that a modified
		     * version of it, can be copied to the header
		     * of each messsage/partial in the collection.
		     */
		    if (!strcasecmp (name, "Subject")) {
			size_t sublen;

			strncpy (subject, buffer, BUFSIZ);
			sublen = strlen (subject);
			if (sublen > 0 && subject[sublen - 1] == '\n')
			    subject[sublen - 1] = '\0';
		    }

		    dp = add (concat (name, ":", buffer, NULL), dp);
		    while (state == FLDPLUS) {
			state = m_getfld (state, name, buffer, sizeof(buffer), in);
			dp = add (buffer, dp);
		    }
		} else {
		    /*
		     * These header fields are copied to the header of
		     * each message/partial in the collection.
		     */
		    cp = add (concat (name, ":", buffer, NULL), cp);
		    while (state == FLDPLUS) {
			state = m_getfld (state, name, buffer, sizeof(buffer), in);
			cp = add (buffer, cp);
		    }
		}

		if (state != FLDEOF) {
		    start = ftell (in) + 1;
		    continue;
		}
		/* else fall... */

	   case BODY:
	   case BODYEOF:
	   case FILEEOF:
		break;

	   case LENERR:
	   case FMTERR:
		adios (NULL, "message format error in component #%d", compnum);

	   default:
		adios (NULL, "getfld () returned %d", state);
	}

	break;
    }
    if (cp == NULL)
	adios (NULL, "headers missing from draft");

    nparts = 1;
    pos = start;
    while (fgets (buffer, sizeof(buffer) - 1, in)) {
	long len;

	if ((pos += (len = strlen (buffer))) > CPERMSG) {
	    nparts++;
	    pos = len;
	}
    }

    /* Only one part, nothing to split */
    if (nparts == 1) {
	free (cp);
	if (dp)
	    free (dp);

	fclose (in);
	return sendaux (vec, vecp, drft, st);
    }

    if (!pushsw) {
	printf ("Sending as %d Partial Messages\n", nparts);
	fflush (stdout);
    }
    status = OK;

    vec[vecp++] = "-partno";
    vec[vecp++] = partnum;
    if (delay == 0)
	vec[vecp++] = "-queued";

    time (&clock);
    snprintf (msgid, sizeof(msgid), "<%d.%ld@%s>",
		(int) getpid(), (long) clock, LocalName());

    fseek (in, start, SEEK_SET);
    for (partno = 1; partno <= nparts; partno++) {
	char tmpdrf[BUFSIZ];
	FILE *out;

	strncpy (tmpdrf, m_scratch (drft, invo_name), sizeof(tmpdrf));
	if ((out = fopen (tmpdrf, "w")) == NULL)
	    adios (tmpdrf, "unable to open for writing");
	chmod (tmpdrf, 0600);

	/*
	 * Output the header fields
	 */
	fputs (cp, out);
	fprintf (out, "Subject: %s (part %d of %d)\n", subject, partno, nparts);
	fprintf (out, "%s: %s\n", VRSN_FIELD, VRSN_VALUE);
	fprintf (out, "%s: message/partial; id=\"%s\";\n", TYPE_FIELD, msgid);
	fprintf (out, "\tnumber=%d; total=%d\n", partno, nparts);
	fprintf (out, "%s: part %d of %d\n\n", DESCR_FIELD, partno, nparts);

	/*
	 * If this is the first in the collection, output the
	 * header fields we are encapsulating at the beginning
	 * of the body of the first message.
	 */
	if (partno == 1) {
	    if (dp)
		fputs (dp, out);
	    fprintf (out, "Message-ID: %s\n", msgid);
	    fprintf (out, "\n");
	}

	pos = 0;
	for (;;) {
	    long len;

	    if (!fgets (buffer, sizeof(buffer) - 1, in)) {
		if (partno == nparts)
		    break;
		adios (NULL, "premature eof");
	    }
	    
	    if ((pos += (len = strlen (buffer))) > CPERMSG) {
		fseek (in, -len, SEEK_CUR);
		break;
	    }

	    fputs (buffer, out);
	}

	if (fflush (out))
	    adios (tmpdrf, "error writing to");

	fclose (out);

	if (!pushsw && verbsw) {
	    printf ("\n");
	    fflush (stdout);
	}

	/* Pause here, if a delay is specified */
	if (delay > 0 && 1 < partno && partno <= nparts) {
	    if (!pushsw) {
		printf ("pausing %d seconds before sending part %d...\n",
			delay, partno);
		fflush (stdout);
	    }
	    sleep ((unsigned int) delay);
	}

	snprintf (partnum, sizeof(partnum), "%d", partno);
	status = sendaux (vec, vecp, tmpdrf, st);
	unlink (tmpdrf);
	if (status != OK)
	    break;

	/*
	 * This is so sendaux will only annotate
	 * the altmsg the first time it is called.
	 */
	annotext = NULL;
    }

    free (cp);
    if (dp)
	free (dp);

    fclose (in);	/* close the draft */
    return status;
}


/*
 * Annotate original message, and
 * call `postproc' to send message.
 */

static int
sendaux (char **vec, int vecp, char *drft, struct stat *st)
{
    pid_t child_id;
    int i, status, fd, fd2;
    char backup[BUFSIZ], buf[BUFSIZ];

    fd = pushsw ? tmp_fd () : NOTOK;
    fd2 = NOTOK;

    vec[vecp++] = drft;
    if (annotext) {
	if ((fd2 = tmp_fd ()) != NOTOK) {
	    vec[vecp++] = "-idanno";
	    snprintf (buf, sizeof(buf), "%d", fd2);
	    vec[vecp++] = buf;
	} else {
	    admonish (NULL, "unable to create file for annotation list");
	}
    }
    if (distfile && distout (drft, distfile, backup) == NOTOK)
	done (1);
    vec[vecp] = NULL;

    for (i = 0; (child_id = vfork()) == NOTOK && i < 5; i++)
	sleep (5);

    switch (child_id) {
    case -1:
	/* oops -- fork error */
	adios ("fork", "unable to");
	break;	/* NOT REACHED */

    case 0:
	/*
	 * child process -- send it
	 *
	 * If fd is ok, then we are pushing and fd points to temp
	 * file, so capture anything on stdout and stderr there.
	 */
	if (fd != NOTOK) {
	    dup2 (fd, fileno (stdout));
	    dup2 (fd, fileno (stderr));
	    close (fd);
	}
	execvp (postproc, vec);
	fprintf (stderr, "unable to exec ");
	perror (postproc);
	_exit (-1);
	break;	/* NOT REACHED */

    default:
	/*
	 * parent process -- wait for it
	 */
	if ((status = pidwait(child_id, NOTOK)) == OK) {
	    if (annotext && fd2 != NOTOK)
		anno (fd2, st);
	} else {
	    /*
	     * If postproc failed, and we have good fd (which means
	     * we pushed), then mail error message (and possibly the
	     * draft) back to the user.
	     */
	    if (fd != NOTOK) {
		alert (drft, fd);
		close (fd);
	    } else {
		advise (NULL, "message not delivered to anyone");
	    }
	    if (annotext && fd2 != NOTOK)
		close (fd2);
	    if (distfile) {
		unlink (drft);
		if (rename (backup, drft) == NOTOK)
		    advise (drft, "unable to rename %s to", backup);
	    }
	}
	break;
    }

    return status;
}


/*
 * Mail error notification (and possibly a copy of the
 * message) back to the user, using the mailproc
 */

static void
alert (char *file, int out)
{
    pid_t child_id;
    int i, in;
    char buf[BUFSIZ];

    for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
	sleep (5);

    switch (child_id) {
	case NOTOK:
	    /* oops -- fork error */
	    advise ("fork", "unable to");

	case OK:
	    /* child process -- send it */
	    SIGNAL (SIGHUP, SIG_IGN);
	    SIGNAL (SIGINT, SIG_IGN);
	    SIGNAL (SIGQUIT, SIG_IGN);
	    SIGNAL (SIGTERM, SIG_IGN);
	    if (forwsw) {
		if ((in = open (file, O_RDONLY)) == NOTOK) {
		    admonish (file, "unable to re-open");
		} else {
		    lseek (out, (off_t) 0, SEEK_END);
		    strncpy (buf, "\nMessage not delivered to anyone.\n", sizeof(buf));
		    write (out, buf, strlen (buf));
		    strncpy (buf, "\n------- Unsent Draft\n\n", sizeof(buf));
		    write (out, buf, strlen (buf));
		    cpydgst (in, out, file, "temporary file");
		    close (in);
		    strncpy (buf, "\n------- End of Unsent Draft\n", sizeof(buf));
		    write (out, buf, strlen (buf));
		    if (rename (file, strncpy (buf, m_backup (file), sizeof(buf))) == NOTOK)
			admonish (buf, "unable to rename %s to", file);
		}
	    }
	    lseek (out, (off_t) 0, SEEK_SET);
	    dup2 (out, fileno (stdin));
	    close (out);
	    /* create subject for error notification */
	    snprintf (buf, sizeof(buf), "send failed on %s",
			forwsw ? "enclosed draft" : file);

	    execlp (mailproc, r1bindex (mailproc, '/'), getusername (),
		    "-subject", buf, NULL);
	    fprintf (stderr, "unable to exec ");
	    perror (mailproc);
	    _exit (-1);

	default: 		/* no waiting... */
	    break;
    }
}


static int
tmp_fd (void)
{
    int fd;
    char tmpfil[BUFSIZ];

    strncpy (tmpfil, m_tmpfil (invo_name), sizeof(tmpfil));
    if ((fd = open (tmpfil, O_RDWR | O_CREAT | O_TRUNC, 0600)) == NOTOK)
	return NOTOK;
    if (debugsw)
	advise (NULL, "temporary file %s selected", tmpfil);
    else
	if (unlink (tmpfil) == NOTOK)
	    advise (tmpfil, "unable to remove");

    return fd;
}


static void
anno (int fd, struct stat *st)
{
    pid_t child_id;
    sigset_t set, oset;
    static char *cwd = NULL;
    struct stat st2;

    if (altmsg &&
	    (stat (altmsg, &st2) == NOTOK
		|| st->st_mtime != st2.st_mtime
		|| st->st_dev != st2.st_dev
		|| st->st_ino != st2.st_ino)) {
	if (debugsw)
	    admonish (NULL, "$mhaltmsg mismatch");
	return;
    }

    child_id = debugsw ? NOTOK : fork ();
    switch (child_id) {
	case NOTOK: 		/* oops */
	    if (!debugsw)
		advise (NULL,
			    "unable to fork, so doing annotations by hand...");
	    if (cwd == NULL)
		cwd = getcpy (pwd ());

	case OK: 
	    /* block a few signals */
	    sigemptyset (&set);
	    sigaddset (&set, SIGHUP);
	    sigaddset (&set, SIGINT);
	    sigaddset (&set, SIGQUIT);
	    sigaddset (&set, SIGTERM);
	    SIGPROCMASK (SIG_BLOCK, &set, &oset);

	    annoaux (fd);
	    if (child_id == OK)
		_exit (0);

	    /* reset the signal mask */
	    SIGPROCMASK (SIG_SETMASK, &oset, &set);

	    chdir (cwd);
	    break;

	default: 		/* no waiting... */
	    close (fd);
	    break;
    }
}


static void
annoaux (int fd)
{
    int	fd2, fd3, msgnum;
    char *cp, *folder, *maildir;
    char buffer[BUFSIZ], **ap;
    FILE *fp;
    struct msgs *mp;

    if ((folder = getenv ("mhfolder")) == NULL || *folder == 0) {
	if (debugsw)
	    admonish (NULL, "$mhfolder not set");
	return;
    }
    maildir = m_maildir (folder);
    if (chdir (maildir) == NOTOK) {
	if (debugsw)
	    admonish (maildir, "unable to change directory to");
	return;
    }
    if (!(mp = folder_read (folder))) {
	if (debugsw)
	    admonish (NULL, "unable to read folder %s");
	return;
    }

    /* check for empty folder */
    if (mp->nummsg == 0) {
	if (debugsw)
	    admonish (NULL, "no messages in %s", folder);
	goto oops;
    }

    if ((cp = getenv ("mhmessages")) == NULL || *cp == 0) {
	if (debugsw)
	    admonish (NULL, "$mhmessages not set");
	goto oops;
    }
    if (!debugsw			/* MOBY HACK... */
	    && pushsw
	    && (fd3 = open ("/dev/null", O_RDWR)) != NOTOK
	    && (fd2 = dup (fileno (stderr))) != NOTOK) {
	dup2 (fd3, fileno (stderr));
	close (fd3);
    }
    else
	fd2 = NOTOK;
    for (ap = brkstring (cp = getcpy (cp), " ", NULL); *ap; ap++)
	m_convert (mp, *ap);
    free (cp);
    if (fd2 != NOTOK)
	dup2 (fd2, fileno (stderr));
    if (mp->numsel == 0) {
	if (debugsw)
	    admonish (NULL, "no messages to annotate");
	goto oops;
    }

    lseek (fd, (off_t) 0, SEEK_SET);
    if ((fp = fdopen (fd, "r")) == NULL) {
	if (debugsw)
	    admonish (NULL, "unable to fdopen annotation list");
	goto oops;
    }
    cp = NULL;
    while (fgets (buffer, sizeof(buffer), fp) != NULL)
	cp = add (buffer, cp);
    fclose (fp);

    if (debugsw)
	advise (NULL, "annotate%s with %s: \"%s\"",
		inplace ? " inplace" : "", annotext, cp);
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (is_selected(mp, msgnum)) {
	    if (debugsw)
		advise (NULL, "annotate message %d", msgnum);
            annotate (m_name (msgnum), annotext, cp, inplace, 1, -2, 0);
	}
    }

    free (cp);

oops:
    folder_free (mp);	/* free folder/message structure */
}


int
done (int status)
{
    if (armed)
	longjmp (env, status ? status : NOTOK);

    exit (status);
    return 1;  /* dead code to satisfy the compiler */
}
