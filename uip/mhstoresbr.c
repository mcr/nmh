
/*
 * mhstoresbr.c -- routines to save/store the contents of MIME messages
 *
 * $Id$
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
#include <setjmp.h>
#include <signal.h>
#include <h/mts.h>
#include <h/tws.h>
#include <h/mime.h>
#include <h/mhparse.h>

extern int errno;

/*
 * The list of top-level contents to display
 */
extern CT *cts;

int autosw = 0;

/*
 * Cache of current directory.  This must be
 * set before these routines are called.
 */
char *cwd;

/*
 * The directory in which to store the contents.
 */
static char *dir;

/*
 * Type for a compare function for qsort.  This keeps
 * the compiler happy.
 */
typedef int (*qsort_comp) (const void *, const void *);


/* mhmisc.c */
int part_ok (CT, int);
int type_ok (CT, int);
int make_intermediates (char *);
void flush_errors (void);

/* mhshowsbr.c */
int show_content_aux (CT, int, int, char *, char *);

/*
 * prototypes
 */
void store_all_messages (CT *);

/*
 * static prototypes
 */
static void store_single_message (CT);
static int store_switch (CT);
static int store_generic (CT);
static int store_application (CT);
static int store_multi (CT);
static int store_partial (CT);
static int store_external (CT);
static int ct_compar (CT *, CT *);
static int store_content (CT, CT);
static int output_content_file (CT, int);
static int check_folder (char *);
static int output_content_folder (char *, char *);
static int parse_format_string (CT, char *, char *, int, char *);
static void get_storeproc (CT);
static int copy_some_headers (FILE *, CT);


/*
 * Main entry point to store content
 * from a collection of messages.
 */

void
store_all_messages (CT *cts)
{
    CT ct, *ctp;
    char *cp;

    /*
     * Check for the directory in which to
     * store any contents.
     */
    if (autosw)
	dir = getcpy (cwd);
    else if ((cp = context_find (nmhstorage)) && *cp)
	dir = getcpy (cp);
    else
	dir = getcpy (cwd);

    for (ctp = cts; *ctp; ctp++) {
	ct = *ctp;
	store_single_message (ct);
    }

    flush_errors ();
}


/*
 * Entry point to store the content
 * in a (single) message
 */

static void
store_single_message (CT ct)
{
    if (type_ok (ct, 1)) {
	umask (ct->c_umask);
	store_switch (ct);
	if (ct->c_fp) {
	    fclose (ct->c_fp);
	    ct->c_fp = NULL;
	}
	if (ct->c_ceclosefnx)
	    (*ct->c_ceclosefnx) (ct);
    }
}


/*
 * Switching routine to store different content types
 */

static int
store_switch (CT ct)
{
    switch (ct->c_type) {
	case CT_MULTIPART:
	    return store_multi (ct);
	    break;

	case CT_MESSAGE:
	    switch (ct->c_subtype) {
		case MESSAGE_PARTIAL:
		    return store_partial (ct);
		    break;

		case MESSAGE_EXTERNAL:
		    return store_external (ct);

		case MESSAGE_RFC822:
		default:
		    return store_generic (ct);
		    break;
	    }
	    break;

	case CT_APPLICATION:
	    return store_application (ct);
	    break;

	case CT_TEXT:
	case CT_AUDIO:
	case CT_IMAGE:
	case CT_VIDEO:
	    return store_generic (ct);
	    break;

	default:
	    adios (NULL, "unknown content type %d", ct->c_type);
	    break;
    }

    return OK;	/* NOT REACHED */
}


/*
 * Generic routine to store a MIME content.
 * (audio, video, image, text, message/rfc922)
 */

static int
store_generic (CT ct)
{
    /*
     * Check if the content specifies a filename.
     * Don't bother with this for type "message"
     * (only "message/rfc822" will use store_generic).
     */
    if (autosw && ct->c_type != CT_MESSAGE)
	get_storeproc (ct);

    return store_content (ct, NULL);
}


/*
 * Store content of type "application"
 */

static int
store_application (CT ct)
{
    char **ap, **ep;
    CI ci = &ct->c_ctinfo;

    /* Check if the content specifies a filename */
    if (autosw)
	get_storeproc (ct);

    /*
     * If storeproc is not defined, and the content is type
     * "application/octet-stream", we also check for various
     * attribute/value pairs which specify if this a tar file.
     */
    if (!ct->c_storeproc && ct->c_subtype == APPLICATION_OCTETS) {
	int tarP = 0, zP = 0, gzP = 0;

	for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++) {
	    /* check for "type=tar" attribute */
	    if (!strcasecmp (*ap, "type")) {
		if (strcasecmp (*ep, "tar"))
		    break;

		tarP = 1;
		continue;
	    }

	    /* check for "conversions=compress" attribute */
	    if ((!strcasecmp (*ap, "conversions") || !strcasecmp (*ap, "x-conversions"))
		&& (!strcasecmp (*ep, "compress") || !strcasecmp (*ep, "x-compress"))) {
		zP = 1;
		continue;
	    }
	    /* check for "conversions=gzip" attribute */
	    if ((!strcasecmp (*ap, "conversions") || !strcasecmp (*ap, "x-conversions"))
		&& (!strcasecmp (*ep, "gzip") || !strcasecmp (*ep, "x-gzip"))) {
		gzP = 1;
		continue;
	    }
	}

	if (tarP) {
	    ct->c_showproc = add (zP ? "%euncompress | tar tvf -"
				  : (gzP ? "%egzip -dc | tar tvf -"
				     : "%etar tvf -"), NULL);
	    if (!ct->c_storeproc) {
		if (autosw) {
		    ct->c_storeproc = add (zP ? "| uncompress | tar xvpf -"
					   : (gzP ? "| gzip -dc | tar xvpf -"
					      : "| tar xvpf -"), NULL);
		    ct->c_umask = 0022;
		} else {
		    ct->c_storeproc= add (zP ? "%m%P.tar.Z"
				          : (gzP ? "%m%P.tar.gz"
					     : "%m%P.tar"), NULL);
		}
	    }
	}
    }

    return store_content (ct, NULL);
}


/*
 * Store the content of a multipart message
 */

static int
store_multi (CT ct)
{
    int	result;
    struct multipart *m = (struct multipart *) ct->c_ctparams;
    struct part *part;

    result = NOTOK;
    for (part = m->mp_parts; part; part = part->mp_next) {
	CT  p = part->mp_part;

	if (part_ok (p, 1) && type_ok (p, 1)) {
	    result = store_switch (p);
	    if (result == OK && ct->c_subtype == MULTI_ALTERNATE)
		break;
	}
    }

    return result;
}


/*
 * Reassemble and store the contents of a collection
 * of messages of type "message/partial".
 */

static int
store_partial (CT ct)
{
    int	cur, hi, i;
    CT p, *ctp, *ctq;
    CT *base;
    struct partial *pm, *qm;

    qm = (struct partial *) ct->c_ctparams;
    if (qm->pm_stored)
	return OK;

    hi = i = 0;
    for (ctp = cts; *ctp; ctp++) {
	p = *ctp;
	if (p->c_type == CT_MESSAGE && p->c_subtype == ct->c_subtype) {
	    pm = (struct partial *) p->c_ctparams;
	    if (!pm->pm_stored
	            && strcmp (qm->pm_partid, pm->pm_partid) == 0) {
		pm->pm_marked = pm->pm_partno;
		if (pm->pm_maxno)
		    hi = pm->pm_maxno;
		pm->pm_stored = 1;
		i++;
	    }
	    else
		pm->pm_marked = 0;
	}
    }

    if (hi == 0) {
	advise (NULL, "missing (at least) last part of multipart message");
	return NOTOK;
    }

    if ((base = (CT *) calloc ((size_t) (i + 1), sizeof(*base))) == NULL)
	adios (NULL, "out of memory");

    ctq = base;
    for (ctp = cts; *ctp; ctp++) {
	p = *ctp;
	if (p->c_type == CT_MESSAGE && p->c_subtype == ct->c_subtype) {
	    pm = (struct partial *) p->c_ctparams;
	    if (pm->pm_marked)
		*ctq++ = p;
	}
    }
    *ctq = NULL;

    if (i > 1)
	qsort ((char *) base, i, sizeof(*base), (qsort_comp) ct_compar);

    cur = 1;
    for (ctq = base; *ctq; ctq++) {
	p = *ctq;
	pm = (struct partial *) p->c_ctparams;
	if (pm->pm_marked != cur) {
	    if (pm->pm_marked == cur - 1) {
		admonish (NULL,
			  "duplicate part %d of %d part multipart message",
			  pm->pm_marked, hi);
		continue;
	    }

missing_part:
	    advise (NULL,
		    "missing %spart %d of %d part multipart message",
		    cur != hi ? "(at least) " : "", cur, hi);
	    goto losing;
	}
        else
	    cur++;
    }
    if (hi != --cur) {
	cur = hi;
	goto missing_part;
    }

    /*
     * Now cycle through the sorted list of messages of type
     * "message/partial" and save/append them to a file.
     */

    ctq = base;
    ct = *ctq++;
    if (store_content (ct, NULL) == NOTOK) {
losing:
	free ((char *) base);
	return NOTOK;
    }

    for (; *ctq; ctq++) {
	p = *ctq;
	if (store_content (p, ct) == NOTOK)
	    goto losing;
    }

    free ((char *) base);
    return OK;
}


/*
 * Store content from a message of type "message/external".
 */

static int
store_external (CT ct)
{
    int	result = NOTOK;
    struct exbody *e = (struct exbody *) ct->c_ctparams;
    CT p = e->eb_content;

    if (!type_ok (p, 1))
	return OK;

    /*
     * Check if the parameters for the external body
     * specified a filename.
     */
    if (autosw) {
	char *cp;

	if ((cp = e->eb_name)
	    && *cp != '/'
	    && *cp != '.'
	    && *cp != '|'
	    && *cp != '!'
	    && !strchr (cp, '%')) {
	    if (!ct->c_storeproc)
		ct->c_storeproc = add (cp, NULL);
	    if (!p->c_storeproc)
		p->c_storeproc = add (cp, NULL);
	}
    }

    /*
     * Since we will let the Content structure for the
     * external body substitute for the current content,
     * we temporarily change its partno (number inside
     * multipart), so everything looks right.
     */
    p->c_partno = ct->c_partno;

    /* we probably need to check if content is really there */
    result = store_switch (p);

    p->c_partno = NULL;
    return result;
}


/*
 * Compare the numbering from two different
 * message/partials (needed for sorting).
 */

static int
ct_compar (CT *a, CT *b)
{
    struct partial *am = (struct partial *) ((*a)->c_ctparams);
    struct partial *bm = (struct partial *) ((*b)->c_ctparams);

    return (am->pm_marked - bm->pm_marked);
}


/*
 * Store contents of a message or message part to
 * a folder, a file, the standard output, or pass
 * the contents to a command.
 *
 * If the current content to be saved is a followup part
 * to a collection of messages of type "message/partial",
 * then field "p" is a pointer to the Content structure
 * to the first message/partial in the group.
 */

static int
store_content (CT ct, CT p)
{
    int appending = 0, msgnum;
    int is_partial = 0, first_partial = 0;
    int last_partial = 0;
    char *cp, buffer[BUFSIZ];

    /*
     * Do special processing for messages of
     * type "message/partial".
     *
     * We first check if this content is of type
     * "message/partial".  If it is, then we need to check
     * whether it is the first and/or last in the group.
     *
     * Then if "p" is a valid pointer, it points to the Content
     * structure of the first partial in the group.  So we copy
     * the file name and/or folder name from that message.  In
     * this case, we also note that we will be appending.
     */
    if (ct->c_type == CT_MESSAGE && ct->c_subtype == MESSAGE_PARTIAL) {
	struct partial *pm = (struct partial *) ct->c_ctparams;

	/* Yep, it's a message/partial */
	is_partial = 1;

	/* But is it the first and/or last in the collection? */
	if (pm->pm_partno == 1)
	    first_partial = 1;
	if (pm->pm_maxno && pm->pm_partno == pm->pm_maxno)
	    last_partial = 1;

	/*
	 * If "p" is a valid pointer, then it points to the
	 * Content structure for the first message in the group.
	 * So we just copy the filename or foldername information
	 * from the previous iteration of this function.
	 */
	if (p) {
	    appending = 1;
	    ct->c_storage = add (p->c_storage, NULL);

	    /* record the folder name */
	    if (p->c_folder) {
		ct->c_folder = add (p->c_folder, NULL);
	    }
	    goto got_filename;
	}
    }

    /*
     * Get storage formatting string.
     *
     * 1) If we have storeproc defined, then use that
     * 2) Else check for a mhn-store-<type>/<subtype> entry
     * 3) Else check for a mhn-store-<type> entry
     * 4) Else if content is "message", use "+" (current folder)
     * 5) Else use string "%m%P.%s".
     */
    if ((cp = ct->c_storeproc) == NULL || *cp == '\0') {
	CI ci = &ct->c_ctinfo;

	snprintf (buffer, sizeof(buffer), "%s-store-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
	if ((cp = context_find (buffer)) == NULL || *cp == '\0') {
	    snprintf (buffer, sizeof(buffer), "%s-store-%s", invo_name, ci->ci_type);
	    if ((cp = context_find (buffer)) == NULL || *cp == '\0') {
		cp = ct->c_type == CT_MESSAGE ? "+" : "%m%P.%s";
	    }
	}
    }

    /*
     * Check the beginning of storage formatting string
     * to see if we are saving content to a folder.
     */
    if (*cp == '+' || *cp == '@') {
	char *tmpfilenam, *folder;

	/* Store content in temporary file for now */
	tmpfilenam = m_scratch ("", invo_name);
	ct->c_storage = add (tmpfilenam, NULL);

	/* Get the folder name */
	if (cp[1])
	    folder = path (cp + 1, *cp == '+' ? TFOLDER : TSUBCWF);
	else
	    folder = getfolder (1);

	/* Check if folder exists */
	if (check_folder (folder) == NOTOK)
	    return NOTOK;

	/* Record the folder name */
	ct->c_folder = add (folder, NULL);

	if (cp[1])
	    free (folder);

	goto got_filename;
    }

    /*
     * Parse and expand the storage formatting string
     * in `cp' into `buffer'.
     */
    parse_format_string (ct, cp, buffer, sizeof(buffer), dir);

    /*
     * If formatting begins with '|' or '!', then pass
     * content to standard input of a command and return.
     */
    if (buffer[0] == '|' || buffer[0] == '!')
	return show_content_aux (ct, 1, 0, buffer + 1, dir);

    /* record the filename */
    ct->c_storage = add (buffer, NULL);

got_filename:
    /* flush the output stream */
    fflush (stdout);

    /* Now save or append the content to a file */
    if (output_content_file (ct, appending) == NOTOK)
	return NOTOK;

    /*
     * If necessary, link the file into a folder and remove
     * the temporary file.  If this message is a partial,
     * then only do this if it is the last one in the group.
     */
    if (ct->c_folder && (!is_partial || last_partial)) {
	msgnum = output_content_folder (ct->c_folder, ct->c_storage);
	unlink (ct->c_storage);
	if (msgnum == NOTOK)
	    return NOTOK;
    }

    /*
     * Now print out the name/number of the message
     * that we are storing.
     */
    if (is_partial) {
	if (first_partial)
	    fprintf (stderr, "reassembling partials ");
	if (last_partial)
	    fprintf (stderr, "%s", ct->c_file);
	else
	    fprintf (stderr, "%s,", ct->c_file);
    } else {
	fprintf (stderr, "storing message %s", ct->c_file);
	if (ct->c_partno)
	    fprintf (stderr, " part %s", ct->c_partno);
    }

    /*
     * Unless we are in the "middle" of group of message/partials,
     * we now print the name of the file, folder, and/or message
     * to which we are storing the content.
     */
    if (!is_partial || last_partial) {
	if (ct->c_folder) {
	    fprintf (stderr, " to folder %s as message %d\n", ct->c_folder, msgnum);
	} else if (!strcmp(ct->c_storage, "-")) {
	    fprintf (stderr, " to stdout\n");
	} else {
	    int cwdlen;

	    cwdlen = strlen (cwd);
	    fprintf (stderr, " as file %s\n",
		strncmp (ct->c_storage, cwd, cwdlen)
		|| ct->c_storage[cwdlen] != '/'
		? ct->c_storage : ct->c_storage + cwdlen + 1);
	}
    }

    return OK;
}


/*
 * Output content to a file
 */

static int
output_content_file (CT ct, int appending)
{
    int filterstate;
    char *file, buffer[BUFSIZ];
    long pos, last;
    FILE *fp;

    /*
     * If the pathname is absolute, make sure
     * all the relevant directories exist.
     */
    if (strchr(ct->c_storage, '/')
	    && make_intermediates (ct->c_storage) == NOTOK)
	return NOTOK;

    if (ct->c_encoding != CE_7BIT) {
	int cc, fd;

	if (!ct->c_ceopenfnx) {
	    advise (NULL, "don't know how to decode part %s of message %s",
		    ct->c_partno, ct->c_file);
	    return NOTOK;
	}

	file = appending || !strcmp (ct->c_storage, "-") ? NULL
							   : ct->c_storage;
	if ((fd = (*ct->c_ceopenfnx) (ct, &file)) == NOTOK)
	    return NOTOK;
	if (!strcmp (file, ct->c_storage)) {
	    (*ct->c_ceclosefnx) (ct);
	    return OK;
	}

	/*
	 * Send to standard output
	 */
	if (!strcmp (ct->c_storage, "-")) {
	    int	gd;

	    if ((gd = dup (fileno (stdout))) == NOTOK) {
		advise ("stdout", "unable to dup");
losing:
		(*ct->c_ceclosefnx) (ct);
		return NOTOK;
	    }
	    if ((fp = fdopen (gd, appending ? "a" : "w")) == NULL) {
		advise ("stdout", "unable to fdopen (%d, \"%s\") from", gd,
			appending ? "a" : "w");
		close (gd);
		goto losing;
	    }
	} else {
	    /*
	     * Open output file
	     */
	    if ((fp = fopen (ct->c_storage, appending ? "a" : "w")) == NULL) {
		advise (ct->c_storage, "unable to fopen for %s",
			appending ? "appending" : "writing");
		goto losing;
	    }
	}

	/*
	 * Filter the header fields of the initial enclosing
	 * message/partial into the file.
	 */
	if (ct->c_type == CT_MESSAGE && ct->c_subtype == MESSAGE_PARTIAL) {
	    struct partial *pm = (struct partial *) ct->c_ctparams;

	    if (pm->pm_partno == 1)
		copy_some_headers (fp, ct);
	}

	for (;;) {
	    switch (cc = read (fd, buffer, sizeof(buffer))) {
		case NOTOK:
		    advise (file, "error reading content from");
		    break;

		case OK:
		    break;

		default:
		    fwrite (buffer, sizeof(*buffer), cc, fp);
		    continue;
	    }
	    break;
	}

	(*ct->c_ceclosefnx) (ct);

	if (cc != NOTOK && fflush (fp))
	    advise (ct->c_storage, "error writing to");

	fclose (fp);

	return (cc != NOTOK ? OK : NOTOK);
    }

    if (!ct->c_fp && (ct->c_fp = fopen (ct->c_file, "r")) == NULL) {
	advise (ct->c_file, "unable to open for reading");
	return NOTOK;
    }

    pos = ct->c_begin;
    last = ct->c_end;
    fseek (ct->c_fp, pos, SEEK_SET);

    if (!strcmp (ct->c_storage, "-")) {
	int gd;

	if ((gd = dup (fileno (stdout))) == NOTOK) {
	    advise ("stdout", "unable to dup");
	    return NOTOK;
	}
	if ((fp = fdopen (gd, appending ? "a" : "w")) == NULL) {
	    advise ("stdout", "unable to fdopen (%d, \"%s\") from", gd,
		    appending ? "a" : "w");
	    close (gd);
	    return NOTOK;
	}
    } else {
	if ((fp = fopen (ct->c_storage, appending ? "a" : "w")) == NULL) {
	    advise (ct->c_storage, "unable to fopen for %s",
		    appending ? "appending" : "writing");
	    return NOTOK;
	}
    }

    /*
     * Copy a few of the header fields of the initial
     * enclosing message/partial into the file.
     */
    filterstate = 0;
    if (ct->c_type == CT_MESSAGE && ct->c_subtype == MESSAGE_PARTIAL) {
	struct partial *pm = (struct partial *) ct->c_ctparams;

	if (pm->pm_partno == 1) {
	    copy_some_headers (fp, ct);
	    filterstate = 1;
	}
    }

    while (fgets (buffer, sizeof(buffer) - 1, ct->c_fp)) {
	if ((pos += strlen (buffer)) > last) {
	    int diff;

	    diff = strlen (buffer) - (pos - last);
	    if (diff >= 0)
		buffer[diff] = '\0';
	}
	/*
	 * If this is the first content of a group of
	 * message/partial contents, then we only copy a few
	 * of the header fields of the enclosed message.
	 */
	if (filterstate) {
	    switch (buffer[0]) {
		case ' ':
		case '\t':
		    if (filterstate < 0)
			buffer[0] = 0;
		    break;

		case '\n':
		    filterstate = 0;
		    break;

		default:
		    if (!uprf (buffer, XXX_FIELD_PRF)
			    && !uprf (buffer, VRSN_FIELD)
			    && !uprf (buffer, "Subject:")
			    && !uprf (buffer, "Encrypted:")
			    && !uprf (buffer, "Message-ID:")) {
			filterstate = -1;
			buffer[0] = 0;
			break;
		    }
		    filterstate = 1;
		    break;
	    }
	}
	fputs (buffer, fp);
	if (pos >= last)
	    break;
    }

    if (fflush (fp))
	advise (ct->c_storage, "error writing to");

    fclose (fp);
    fclose (ct->c_fp);
    ct->c_fp = NULL;
    return OK;
}


/*
 * Check if folder exists, and create
 * if necessary.
 */

static int
check_folder (char *folder)
{
    char *folderdir;
    struct stat st;

    /* expand path to the folder */
    folderdir = m_mailpath (folder);

    /* Check if folder exists */
    if (stat (folderdir, &st) == NOTOK) {
	int answer;
	char *ep;

	if (errno != ENOENT) {
	    advise (folderdir, "error on folder");
	    return NOTOK;
	}

	ep = concat ("Create folder \"", folderdir, "\"? ", NULL);
	answer = getanswer (ep);
	free (ep);

	if (!answer)
	    return NOTOK;

	if (!makedir (folderdir)) {
	    advise (NULL, "unable to create folder %s", folderdir);
	    return NOTOK;
	}
    }

    return OK;
}


/*
 * Add a file to a folder.
 *
 * Return the new message number of the file
 * when added to the folder.  Return -1, if
 * there is an error.
 */

static int
output_content_folder (char *folder, char *filename)
{
    int msgnum;
    struct msgs *mp;

    /* Read the folder. */
    if ((mp = folder_read (folder))) {
	/* Link file into folder */
	msgnum = folder_addmsg (&mp, filename, 0, 0, 0);
    } else {
	advise (NULL, "unable to read folder %s", folder);
	return NOTOK;
    }

    /* free folder structure */
    folder_free (mp);

    /*
     * Return msgnum.  We are relying on the fact that
     * msgnum will be -1, if folder_addmsg() had an error.
     */
    return msgnum;
}


/*
 * Parse and expand the storage formatting string
 * pointed to by "cp" into "buffer".
 */

static int
parse_format_string (CT ct, char *cp, char *buffer, int buflen, char *dir)
{
    int len;
    char *bp;
    CI ci = &ct->c_ctinfo;

    /*
     * If storage string is "-", just copy it, and
     * return (send content to standard output).
     */
    if (cp[0] == '-' && cp[1] == '\0') {
	strncpy (buffer, cp, buflen);
	return 0;
    }

    bp = buffer;
    bp[0] = '\0';

    /*
     * If formatting string is a pathname that doesn't
     * begin with '/', then preface the path with the
     * appropriate directory.
     */
    if (*cp != '/' && *cp != '|' && *cp != '!') {
	snprintf (bp, buflen, "%s/", dir[1] ? dir : "");
	len = strlen (bp);
	bp += len;
	buflen -= len;
    }

    for (; *cp; cp++) {

	/* We are processing a storage escape */
	if (*cp == '%') {
	    switch (*++cp) {
		case 'a':
		    /*
		     * Insert parameters from Content-Type.
		     * This is only valid for '|' commands.
		     */
		    if (buffer[0] != '|' && buffer[0] != '!') {
			*bp++ = *--cp;
			*bp = '\0';
			buflen--;
			continue;
		    } else {
			char **ap, **ep;
			char *s = "";

			for (ap = ci->ci_attrs, ep = ci->ci_values;
			         *ap; ap++, ep++) {
			    snprintf (bp, buflen, "%s%s=\"%s\"", s, *ap, *ep);
			    len = strlen (bp);
			    bp += len;
			    buflen -= len;
			    s = " ";
			}
		    }
		    break;

		case 'm':
		    /* insert message number */
		    snprintf (bp, buflen, "%s", r1bindex (ct->c_file, '/'));
		    break;

		case 'P':
		    /* insert part number with leading dot */
		    if (ct->c_partno)
			snprintf (bp, buflen, ".%s", ct->c_partno);
		    break;

		case 'p':
		    /* insert part number withouth leading dot */
		    if (ct->c_partno)
			strncpy (bp, ct->c_partno, buflen);
		    break;

		case 't':
		    /* insert content type */
		    strncpy (bp, ci->ci_type, buflen);
		    break;

		case 's':
		    /* insert content subtype */
		    strncpy (bp, ci->ci_subtype, buflen);
		    break;

		case '%':
		    /* insert the character % */
		    goto raw;

		default:
		    *bp++ = *--cp;
		    *bp = '\0';
		    buflen--;
		    continue;
	    }

	    /* Advance bp and decrement buflen */
	    len = strlen (bp);
	    bp += len;
	    buflen -= len;

	} else {
raw:
	    *bp++ = *cp;
	    *bp = '\0';
	    buflen--;
	}
    }

    return 0;
}


/*
 * Check if the content specifies a filename
 * in its MIME parameters.
 */

static void
get_storeproc (CT ct)
{
    char **ap, **ep, *cp;
    CI ci = &ct->c_ctinfo;

    /*
     * If the storeproc has already been defined,
     * we just return (for instance, if this content
     * is part of a "message/external".
     */
    if (ct->c_storeproc)
	return;

    /*
     * Check the attribute/value pairs, for the attribute "name".
     * If found, do a few sanity checks and copy the value into
     * the storeproc.
     */
    for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++) {
	if (!strcasecmp (*ap, "name")
	    && *(cp = *ep) != '/'
	    && *cp != '.'
	    && *cp != '|'
	    && *cp != '!'
	    && !strchr (cp, '%')) {
	    ct->c_storeproc = add (cp, NULL);
	    return;
	}
    }
}


/*
 * Copy some of the header fields of the initial message/partial
 * message into the header of the reassembled message.
 */

static int
copy_some_headers (FILE *out, CT ct)
{
    HF hp;

    hp = ct->c_first_hf;	/* start at first header field */

    while (hp) {
	/*
	 * A few of the header fields of the enclosing
	 * messages are not copied.
	 */
	if (!uprf (hp->name, XXX_FIELD_PRF)
		&& strcasecmp (hp->name, VRSN_FIELD)
		&& strcasecmp (hp->name, "Subject")
		&& strcasecmp (hp->name, "Encrypted")
		&& strcasecmp (hp->name, "Message-ID"))
	    fprintf (out, "%s:%s", hp->name, hp->value);
	hp = hp->next;	/* next header field */
    }

    return OK;
}
