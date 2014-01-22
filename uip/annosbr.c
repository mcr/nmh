
/*
 * annosbr.c -- prepend annotation to messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/tws.h>
#include <h/utils.h>
#include <fcntl.h>
#include <utime.h>


/*
 * static prototypes
 */
static int annosbr (int, char *, char *, char *, int, int, int, int);

/*
 *	This "local" global and the annopreserve() function are a hack that allows additional
 *	functionality to be added to anno without piling on yet another annotate() argument.
 */

static	int	preserve_actime_and_modtime = 0;	/* set to preserve access and modification times on annotated message */

int
annotate (char *file, char *comp, char *text, int inplace, int datesw, int delete, int append)
{
    int			i, fd;
    struct utimbuf	b;
    struct stat		s;

    /* open and lock the file to be annotated */
    if ((fd = lkopendata (file, O_RDWR, 0)) == NOTOK) {
	switch (errno) {
	    case ENOENT:
		break;

	    default:
		admonish (file, "unable to lock and open");
		break;
	}
	return 1;
    }

    if (stat(file, &s) == -1) {
	advise("can't get access and modification times for %s", file);
    	preserve_actime_and_modtime = 0;
    }

    b.actime = s.st_atime;
    b.modtime = s.st_mtime;

    i = annosbr (fd, file, comp, text, inplace, datesw, delete, append);

    if (preserve_actime_and_modtime && utime(file, &b) == -1)
	advise("can't set access and modification times for %s", file);

    lkclosedata (fd, file);
    return i;
}

/*
 *  Produce a listing of all header fields (annotations) whose field name matches
 *  comp.  Number the listing if number is set.  Treate the field bodies as path
 *  names and just output the last component unless text is non-NULL.  We don't
 *  care what text is set to.
 */

void
annolist(char *file, char *comp, char *text, int number)
{
    int		c;		/* current character */
    int		count;		/* header field (annotation) counter */
    char	*cp;		/* miscellaneous character pointer */
    char	*field;		/* buffer for header field */
    int		field_size;	/* size of field buffer */
    FILE	*fp;		/* file pointer made from locked file descriptor */
    int		length;		/* length of field name */
    int		n;		/* number of bytes written */
    char	*sp;		/* another miscellaneous character pointer */

    if ((fp = fopen(file, "r")) == (FILE *)0)
	adios(file, "unable to open");

    /*
     *  Allocate a buffer to hold the header components as they're read in.
     *  This buffer might need to be quite large, so we grow it as needed.
     */

    field = (char *)mh_xmalloc(field_size = 256);

    /*
     *  Get the length of the field name since we use it often.
     */

    length = strlen(comp);

    count = 0;

    do {
	/*
	 *	Get a line from the input file, growing the field buffer as needed.  We do this
	 *	so that we can fit an entire line in the buffer making it easy to do a string
	 *	comparison on both the field name and the field body which might be a long path
	 *	name.
	 */

	for (n = 0, cp = field; (c = getc(fp)) != EOF; *cp++ = c) {
	    if (c == '\n' && (c = getc(fp)) != ' ' && c != '\t') {
		(void)ungetc(c, fp);
		c = '\n';
		break;
	    }

	    if (++n >= field_size - 1) {
		field = (char *) mh_xrealloc((void *)field, field_size += 256);
		
		cp = field + n - 1;
	    }
	}

	/*
	 *	NUL-terminate the field..
	 */

	*cp = '\0';

	if (strncasecmp(field, comp, length) == 0 && field[length] == ':') {
	    for (cp = field + length + 1; *cp == ' ' || *cp == '\t'; cp++)
		;

	    if (number)
		(void)printf("%d\t", ++count);

	    if (text == (char *)0 && (sp = strrchr(cp, '/')) != (char *)0)
		cp = sp + 1;

	    (void)printf("%s\n", cp);
	}

    } while (*field != '\0' && *field != '-');

    /*
     *	Clean up.
     */

    free(field);

    (void)fclose(fp);

    return;
}

/*
 *	Set the preserve-times flag.  This hack eliminates the need for an additional argument to annotate().
 */

void
annopreserve(int preserve)
{
	preserve_actime_and_modtime = preserve;
	return;
}

static int
annosbr (int fd, char *file, char *comp, char *text, int inplace, int datesw, int delete, int append)
{
    int mode, tmpfd;
    char *cp, *sp;
    char buffer[BUFSIZ], tmpfil[BUFSIZ];
    struct stat st;
    FILE	*tmp;
    int		c;		/* current character */
    int		count;		/* header field (annotation) counter */
    char	*field = NULL;	/* buffer for header field */
    int		field_size = 0;	/* size of field buffer */
    FILE	*fp = NULL;	/* file pointer made from locked file descriptor */
    int		length;		/* length of field name */
    int		n;		/* number of bytes written */

    mode = fstat (fd, &st) != NOTOK ? (int) (st.st_mode & 0777) : m_gmprot ();

    if ((cp = m_mktemp2(file, "annotate", NULL, &tmp)) == NULL) {
	adios(NULL, "unable to create temporary file in %s", get_temp_dir());
    }
    strncpy (tmpfil, cp, sizeof(tmpfil));
    chmod (tmpfil, mode);

    /*
     *  We're going to need to copy some of the message file to the temporary
     *	file while examining the contents.  Convert the message file descriptor
     *	to a file pointer since it's a lot easier and more efficient to use
     *	stdio for this.  Also allocate a buffer to hold the header components
     *	as they're read in.  This buffer is grown as needed later.
     */

    if (delete >= -1 || append != 0) {
	if ((fp = fdopen(fd, "r")) == (FILE *)0)
	    adios(NULL, "unable to fdopen file.");

	field = (char *)mh_xmalloc(field_size = 256);
    }

    /*
     *	We're trying to delete a header field (annotation )if the delete flag is
     *	not -2 or less.  A  value greater than zero means that we're deleting the
     *	nth header field that matches the field (component) name.  A value of
     *	zero means that we're deleting the first field in which both the field
     *	name matches the component name and the field body matches the text.
     *	The text is matched in its entirety if it begins with a slash; otherwise
     *	the text is matched against whatever portion of the field body follows
     *	the last slash.  This allows matching of both absolute and relative path
     *	names.  This is because this functionality was added to support attachments.
     *	It might be worth having a separate flag to indicate path name matching to
     *	make it more general.  A value of -1 means to delete all matching fields.
     */

    if (delete >= -1) {
	/*
	 *  Get the length of the field name since we use it often.
	 */

	length = strlen(comp);

	/*
	 *  Initialize the field counter.  This is only used if we're deleting by
	 *  number.
	 */

	count = 0;

	/*
	 *  Copy lines from the input file to the temporary file until we either find the one
	 *  that we're looking for (which we don't copy) or we reach the end of the headers.
	 *  Both a blank line and a line beginning with a - terminate the headers so that we
	 *  can handle both drafts and RFC-2822 format messages.
	 */

	do {
	    /*
	     *	Get a line from the input file, growing the field buffer as needed.  We do this
	     *	so that we can fit an entire line in the buffer making it easy to do a string
	     *	comparison on both the field name and the field body which might be a long path
	     *	name.
	     */

	    for (n = 0, cp = field; (c = getc(fp)) != EOF; *cp++ = c) {
		if (c == '\n' && (c = getc(fp)) != ' ' && c != '\t') {
		    (void)ungetc(c, fp);
		    c = '\n';
		    break;
		}

		if (++n >= field_size - 1) {
		    field = (char *) mh_xrealloc((void *)field, field_size *= 2);
		
		    cp = field + n - 1;
		}
	    }

	    /*
	     *	NUL-terminate the field..
	     */

	    *cp = '\0';

	    /*
	     *	Check for a match on the field name.  We delete the line by not copying it to the
	     *	temporary file if
	     *
	     *	 o  The delete flag is 0, meaning that we're going to delete the first matching
	     *	    field, and the text is NULL meaning that we don't care about the field body.
	     *
	     *	 o  The delete flag is 0, meaning that we're going to delete the first matching
	     *	    field, and the text begins with a / meaning that we're looking for a full
	     *	    path name, and the text matches the field body.
	     *
	     *	 o  The delete flag is 0, meaning that we're going to delete the first matching
	     *	    field, the text does not begin with a / meaning that we're looking for the
	     *	    last path name component, and the last path name component matches the text.
	     *
	     *   o  The delete flag is positive meaning that we're going to delete the nth field
	     *	    with a matching field name, and this is the nth matching field name.
	     *
	     *   o  The delete flag is -1 meaning that we're going to delete all fields with a
	     *      matching field name.
	     */

	    if (strncasecmp(field, comp, length) == 0 && field[length] == ':') {
		if (delete == 0) {
		    if (text == (char *)0)
			break;

		    for (cp = field + length + 1; *cp == ' ' || *cp == '\t'; cp++)
			;

		    if (*text == '/') {
			if (strcmp(cp, text) == 0)
				break;
		    }
		    else {
			if ((sp = strrchr(cp, '/')) != (char *)0)
			    cp = sp + 1;

			if (strcmp(cp, text) == 0)
			    break;
		    }
		}

		else if (delete == -1)
			continue;

		else if (++count == delete)
		    break;
	    }

	    /*
	     *	This line wasn't a match so copy it to the temporary file.
	     */

	    if ((n = fputs(field, tmp)) == EOF || (c == '\n' && fputc('\n', tmp) == EOF))
		adios(NULL, "unable to write temporary file.");

	} while (*field != '\0' && *field != '-');

	/*
	 *  Get rid of the field buffer because we're done with it.
	 */

	free((void *)field);
    }

    else {
	/*
	 *  Find the end of the headers before adding the annotations if we're
	 *  appending instead of the default prepending.  A special check for
	 *  no headers is needed if appending.
	 */

	if (append) {
	    /*
	     *	Copy lines from the input file to the temporary file until we
	     *  reach the end of the headers.
	     */

	    if ((c = getc(fp)) == '\n')
		rewind(fp);

	    else {
	        (void)putc(c, tmp);

	        while ((c = getc(fp)) != EOF) {
		    (void)putc(c, tmp);

		    if (c == '\n') {
		        (void)ungetc(c = getc(fp), fp);

		        if (c == '\n' || c == '-')
			    break;
		    }
		}
	    }
	}

	if (datesw)
	    fprintf (tmp, "%s: %s\n", comp, dtimenow (0));
	if ((cp = text)) {
	    do {
		while (*cp == ' ' || *cp == '\t')
		    cp++;
		sp = cp;
		while (*cp && *cp++ != '\n')
		    continue;
		if (cp - sp)
		    fprintf (tmp, "%s: %*.*s", comp, (int)(cp - sp), (int)(cp - sp), sp);
	    } while (*cp);
	    if (cp[-1] != '\n' && cp != text)
		putc ('\n', tmp);
	}
    }

    fflush (tmp);

    /*
     *	We've been messing with the input file position.  Move the input file
     *  descriptor to the current place in the file because the stock data
     *	copying routine uses the descriptor, not the pointer.
     */

    if (append || delete >= -1) {
	if (lseek(fd, (off_t)ftell(fp), SEEK_SET) == (off_t)-1)
	    adios(NULL, "can't seek.");
    }

    cpydata (fd, fileno (tmp), file, tmpfil);
    fclose (tmp);

    if (inplace) {
	if ((tmpfd = open (tmpfil, O_RDONLY)) == NOTOK)
	    adios (tmpfil, "unable to open for re-reading");

	lseek (fd, (off_t) 0, SEEK_SET);

	/*
	 *  We're making the file shorter if we're deleting a header field
	 *  so the file has to be truncated or it will contain garbage.
	 */

	if (delete >= -1 && ftruncate(fd, 0) == -1)
	    adios(tmpfil, "unable to truncate.");

	cpydata (tmpfd, fd, tmpfil, file);
	close (tmpfd);
	unlink (tmpfil);
    } else {
	strncpy (buffer, m_backup (file), sizeof(buffer));
	if (rename (file, buffer) == NOTOK) {
	    switch (errno) {
		case ENOENT:	/* unlinked early - no annotations */
		    unlink (tmpfil);
		    break;

		default:
		    admonish (buffer, "unable to rename %s to", file);
		    break;
	    }
	    return 1;
	}
	if (rename (tmpfil, file) == NOTOK) {
	    admonish (file, "unable to rename %s to", tmpfil);
	    return 1;
	}
    }

    /*
     *	Close the delete file so that we don't run out of file pointers if
     *	we're doing piles of files.  Note that this will make the close() in
     *	lkclose() fail, but that failure is ignored so it's not a problem.
     */

    if (delete >= -1)
	(void)fclose(fp);

    return 0;
}
