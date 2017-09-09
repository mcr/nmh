/* mhstoresbr.c -- routines to save/store the contents of MIME messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/md5.h>
#include <h/mts.h>
#include <h/tws.h>
#include <h/fmt_scan.h>
#include <h/mime.h>
#include <h/mhparse.h>
#include <h/utils.h>
#include "mhmisc.h"
#include "mhshowsbr.h"
#include "sbr/m_maildir.h"
#include "sbr/m_mktemp.h"

enum clobber_policy_t {
  NMH_CLOBBER_ALWAYS = 0,
  NMH_CLOBBER_AUTO,
  NMH_CLOBBER_SUFFIX,
  NMH_CLOBBER_ASK,
  NMH_CLOBBER_NEVER
};

static enum clobber_policy_t clobber_policy (const char *) PURE;

struct mhstoreinfo {
    CT *cts;                 /* Top-level list of contents to store. */
    char *cwd;               /* cached current directory */
    int autosw;              /* -auto enabled */
    int verbosw;             /* -verbose enabled */
    int files_not_clobbered; /* output flag indicating that store failed
                                in order to not clobber an existing file */

    /* The following must never be touched by a caller:  they are for
       internal use by the mhstoresbr functions. */
    char *dir;               /* directory in which to store contents */
    enum clobber_policy_t clobber_policy;  /* -clobber selection */
};

static bool use_param_as_filename(const char *p);

mhstoreinfo_t
mhstoreinfo_create (CT *ct, char *pwd, const char *csw, int asw, int vsw) {
    mhstoreinfo_t info;

    NEW(info);
    info->cts = ct;
    info->cwd = pwd;
    info->autosw = asw;
    info->verbosw = vsw;
    info->files_not_clobbered = 0;
    info->dir = NULL;
    info->clobber_policy = clobber_policy (csw);

    return info;
}

void
mhstoreinfo_free (mhstoreinfo_t info) {
    free (info->cwd);
    free (info->dir);
    free (info);
}

int
mhstoreinfo_files_not_clobbered (const mhstoreinfo_t info) {
    return info->files_not_clobbered;
}


/*
 * Type for a compare function for qsort.  This keeps
 * the compiler happy.
 */
typedef int (*qsort_comp) (const void *, const void *);


/*
 * static prototypes
 */
static void store_single_message (CT, mhstoreinfo_t);
static int store_switch (CT, mhstoreinfo_t);
static int store_generic (CT, mhstoreinfo_t);
static int store_application (CT, mhstoreinfo_t);
static int store_multi (CT, mhstoreinfo_t);
static int store_partial (CT, mhstoreinfo_t);
static int store_external (CT, mhstoreinfo_t);
static int ct_compar (CT *, CT *);
static int store_content (CT, CT, mhstoreinfo_t);
static int output_content_file (CT, int);
static int output_content_folder (char *, char *);
static int parse_format_string (CT, char *, char *, int, char *);
static void get_storeproc (CT);
static int copy_some_headers (FILE *, CT);
static char *clobber_check (char *, mhstoreinfo_t);

/*
 * Main entry point to store content
 * from a collection of messages.
 */

void
store_all_messages (mhstoreinfo_t info)
{
    CT ct, *ctp;
    char *cp;

    /*
     * Check for the directory in which to
     * store any contents.
     */
    if ((cp = context_find (nmhstorage)) && *cp)
	info->dir = mh_xstrdup(cp);
    else
	info->dir = getcpy (info->cwd);

    for (ctp = info->cts; *ctp; ctp++) {
	ct = *ctp;
	store_single_message (ct, info);
    }

    flush_errors ();
}


/*
 * Entry point to store the content
 * in a (single) message
 */

static void
store_single_message (CT ct, mhstoreinfo_t info)
{
    if (type_ok (ct, 1)) {
	umask (ct->c_umask);
	store_switch (ct, info);
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
store_switch (CT ct, mhstoreinfo_t info)
{
    switch (ct->c_type) {
	case CT_MULTIPART:
	    return store_multi (ct, info);

	case CT_MESSAGE:
	    switch (ct->c_subtype) {
		case MESSAGE_PARTIAL:
		    return store_partial (ct, info);

		case MESSAGE_EXTERNAL:
		    return store_external (ct, info);

		case MESSAGE_RFC822:
		default:
		    return store_generic (ct, info);
	    }

	case CT_APPLICATION:
	default:
	    return store_application (ct, info);

	case CT_TEXT:
	case CT_AUDIO:
	case CT_IMAGE:
	case CT_VIDEO:
	    return store_generic (ct, info);
    }

    return OK;	/* NOT REACHED */
}


/*
 * Generic routine to store a MIME content.
 * (audio, video, image, text, message/rfc822)
 */

static int
store_generic (CT ct, mhstoreinfo_t info)
{
    /*
     * Check if the content specifies a filename.
     * Don't bother with this for type "message"
     * (only "message/rfc822" will use store_generic).
     */
    if (info->autosw && ct->c_type != CT_MESSAGE)
	get_storeproc (ct);

    return store_content (ct, NULL, info);
}


/*
 * Store content of type "application"
 */

static int
store_application (CT ct, mhstoreinfo_t info)
{
    CI ci = &ct->c_ctinfo;

    /* Check if the content specifies a filename */
    if (info->autosw)
	get_storeproc (ct);

    /*
     * If storeproc is not defined, and the content is type
     * "application/octet-stream", we also check for various
     * attribute/value pairs which specify if this a tar file.
     */
    if (!ct->c_storeproc && ct->c_subtype == APPLICATION_OCTETS) {
	int tarP = 0, zP = 0, gzP = 0;
	char *cp;

	if ((cp = get_param(ci->ci_first_pm, "type", ' ', 1))) {
	    if (strcasecmp (cp, "tar") == 0)
		tarP = 1;
	}

	/* check for "conversions=compress" attribute */
	if ((cp = get_param(ci->ci_first_pm, "conversions", ' ', 1)) ||
	    (cp = get_param(ci->ci_first_pm, "x-conversions", ' ', 1))) {
	    if (strcasecmp (cp, "compress") == 0 ||
		    strcasecmp (cp, "x-compress") == 0) {
		zP = 1;
	    }
	    if (strcasecmp (cp, "gzip") == 0 ||
		    strcasecmp (cp, "x-gzip") == 0) {
		gzP = 1;
	    }
	}

	if (tarP) {
	    ct->c_showproc = add (zP ? "%euncompress | tar tvf -"
				  : (gzP ? "%egzip -dc | tar tvf -"
				     : "%etar tvf -"), NULL);
	    if (!ct->c_storeproc) {
		if (info->autosw) {
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

    return store_content (ct, NULL, info);
}


/*
 * Store the content of a multipart message
 */

static int
store_multi (CT ct, mhstoreinfo_t info)
{
    int	result;
    struct multipart *m = (struct multipart *) ct->c_ctparams;
    struct part *part;

    result = NOTOK;
    for (part = m->mp_parts; part; part = part->mp_next) {
	CT  p = part->mp_part;

	if (part_ok (p) && type_ok (p, 1)) {
	    if (ct->c_storage) {
		/* Support mhstore -outfile.  The MIME parser doesn't
		   load c_storage, so we know that p->c_storage is
		   NULL here. */
		p->c_storage = mh_xstrdup(ct->c_storage);
	    }
	    result = store_switch (p, info);

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
store_partial (CT ct, mhstoreinfo_t info)
{
    int	cur, hi, i;
    CT p, *ctp, *ctq;
    CT *base;
    struct partial *pm, *qm;

    qm = (struct partial *) ct->c_ctparams;
    if (qm->pm_stored)
	return OK;

    hi = i = 0;
    for (ctp = info->cts; *ctp; ctp++) {
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
	inform("missing (at least) last part of multipart message");
	return NOTOK;
    }

    base = mh_xcalloc(i + 1, sizeof *base);
    ctq = base;
    for (ctp = info->cts; *ctp; ctp++) {
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
	if (pm->pm_marked == cur) {
	    cur++;
            continue;
        }

        if (pm->pm_marked == cur - 1) {
            inform("duplicate part %d of %d part multipart message, continuing...",
                      pm->pm_marked, hi);
            continue;
        }

missing_part:
        inform("missing %spart %d of %d part multipart message",
            cur != hi ? "(at least) " : "", cur, hi);
        goto losing;
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
    if (store_content (ct, NULL, info) == NOTOK) {
losing:
	free(base);
	return NOTOK;
    }

    for (; *ctq; ctq++) {
	p = *ctq;
	if (store_content (p, ct, info) == NOTOK)
	    goto losing;
    }

    free(base);
    return OK;
}


/*
 * Store content from a message of type "message/external".
 */

static int
store_external (CT ct, mhstoreinfo_t info)
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
    if (info->autosw) {
	char *cp;

	if ((cp = e->eb_name) && use_param_as_filename(cp)) {
	    if (!ct->c_storeproc)
		ct->c_storeproc = mh_xstrdup(cp);
	    if (!p->c_storeproc)
		p->c_storeproc = mh_xstrdup(cp);
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
    if (ct->c_storage) {
	/* Support mhstore -outfile.  The MIME parser doesn't load
	   c_storage, so we know that p->c_storage is NULL here. */
	p->c_storage = mh_xstrdup(ct->c_storage);
    }
    result = store_switch (p, info);

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
store_content (CT ct, CT p, mhstoreinfo_t info)
{
    int appending = 0, msgnum = 0;
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
            if (! ct->c_storage) {
		ct->c_storage = mh_xstrdup(FENDNULL(p->c_storage));

		/* record the folder name */
		if (p->c_folder) {
		    ct->c_folder = mh_xstrdup(p->c_folder);
		}
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

	cp = context_find_by_type ("store", ci->ci_type, ci->ci_subtype);
	if (cp == NULL) {
	    cp = ct->c_type == CT_MESSAGE ? "+" : "%m%P.%s";
	}
    }

    if (! ct->c_storage) {
	/*
	 * Check the beginning of storage formatting string
	 * to see if we are saving content to a folder.
	 */
	if (*cp == '+' || *cp == '@') {
	    char *tmpfilenam, *folder;

	    /* Store content in temporary file for now */
	    if ((tmpfilenam = m_mktemp(invo_name, NULL, NULL)) == NULL) {
		adios(NULL, "unable to create temporary file in %s",
		      get_temp_dir());
	    }
	    ct->c_storage = mh_xstrdup(tmpfilenam);

	    /* Get the folder name */
	    if (cp[1])
		folder = pluspath (cp);
	    else
		folder = getfolder (1);

	    /* Check if folder exists */
	    create_folder(m_mailpath(folder), 0, exit);

	    /* Record the folder name */
	    ct->c_folder = mh_xstrdup(folder);

	    if (cp[1])
		free (folder);

	    goto got_filename;
	}

	/*
	 * Parse and expand the storage formatting string
	 * in `cp' into `buffer'.
	 */
	parse_format_string (ct, cp, buffer, sizeof(buffer), info->dir);

	/*
	 * If formatting begins with '|' or '!', then pass
	 * content to standard input of a command and return.
	 */
	if (buffer[0] == '|' || buffer[0] == '!')
	    return show_content_aux (ct, 0, buffer + 1, info->dir, NULL);

        /* record the filename */
	if ((ct->c_storage = clobber_check (mh_xstrdup(buffer), info)) ==
	    NULL) {
	    return NOTOK;
	}
    } else {
	/* The output filename was explicitly specified, so use it. */
	if ((ct->c_storage = clobber_check (ct->c_storage, info)) ==
	    NULL) {
	    return NOTOK;
	}
    }

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
	(void) m_unlink (ct->c_storage);
	if (msgnum == NOTOK)
	    return NOTOK;
    }

    if (info->verbosw) {
        /*
         * Now print out the name/number of the message
         * that we are storing.
         */
        if (is_partial) {
            if (first_partial)
                fprintf (stderr, "reassembling partials ");
            if (last_partial)
                fputs(ct->c_file, stderr);
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
                fprintf (stderr, " to folder %s as message %d\n", ct->c_folder,
                         msgnum);
            } else if (!strcmp(ct->c_storage, "-")) {
                fprintf (stderr, " to stdout\n");
            } else {
                int cwdlen = strlen (info->cwd);

                fprintf (stderr, " as file %s\n",
                         !has_prefix(ct->c_storage, info->cwd)
                         || ct->c_storage[cwdlen] != '/'
                         ? ct->c_storage : ct->c_storage + cwdlen + 1);
            }
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
	    inform("don't know how to decode part %s of message %s",
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
		    if ((int) fwrite (buffer, sizeof(*buffer), cc, fp) < cc) {
			advise ("output_content_file", "fwrite");
		    }
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

    while (fgets (buffer, sizeof buffer, ct->c_fp)) {
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
    if (!(mp = folder_read(folder, 0))) {
	inform("unable to read folder %s", folder);
	return NOTOK;
    }
    /* Link file into folder */
    msgnum = folder_addmsg(&mp, filename, 0, 0, 0, 0, NULL);

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
        if (!strcmp(dir, "/"))
            dir = ""; /* Don't start with "//". */
	snprintf (bp, buflen, "%s/", dir);
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
		    }
                    {
			PM pm;
			char *s = "";

			for (pm = ci->ci_first_pm; pm; pm = pm->pm_next) {
			    snprintf (bp, buflen, "%s%s=\"%s\"", s,
				      pm->pm_name, get_param_value(pm, '?'));
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
		    /* insert part number without leading dot */
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
    char *cp;
    CI ci;

    /*
     * If the storeproc has already been defined,
     * we just return (for instance, if this content
     * is part of a "message/external".
     */
    if (ct->c_storeproc)
	return;

    /*
     * If there's a Content-Disposition header and it has a filename,
     * use that (RFC-2183).
     */
    if (ct->c_dispo) {
	if ((cp = get_param(ct->c_dispo_first, "filename", '_', 0)) &&
            use_param_as_filename(cp)) {
		ct->c_storeproc = mh_xstrdup(cp);
		free(cp);
		return;
	}
        mh_xfree(cp);
    }

    /*
     * Check the attribute/value pairs, for the attribute "name".
     * If found, do a few sanity checks and copy the value into
     * the storeproc.
     */
    ci = &ct->c_ctinfo;
    if ((cp = get_param(ci->ci_first_pm, "name", '_', 0)) &&
        use_param_as_filename(cp)) {
	    ct->c_storeproc = mh_xstrdup(cp);

    }
    mh_xfree(cp);
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

/******************************************************************************/
/* -clobber support */

static
enum clobber_policy_t
clobber_policy (const char *value) {
  if (value == NULL  ||  ! strcasecmp (value, "always")) {
    return NMH_CLOBBER_ALWAYS;
  }
  if (! strcasecmp (value, "auto")) {
    return NMH_CLOBBER_AUTO;
  }
  if (! strcasecmp (value, "suffix")) {
    return NMH_CLOBBER_SUFFIX;
  }
  if (! strcasecmp (value, "ask")) {
    return NMH_CLOBBER_ASK;
  }
  if (! strcasecmp (value, "never")) {
    return NMH_CLOBBER_NEVER;
  }

  adios (NULL, "invalid argument, %s, to clobber", value);
}


static char *
next_version (char *file, enum clobber_policy_t clobber_policy) {
  const size_t max_versions = 1000000;
  /* 8 = log max_versions  +  one for - or .  +  one for null terminator */
  const size_t buflen = strlen (file) + 8;
  char *buffer = mh_xmalloc (buflen);
  size_t version;

  char *extension = NULL;
  if (clobber_policy == NMH_CLOBBER_AUTO  &&
      ((extension = strrchr (file, '.')) != NULL)) {
    *extension++ = '\0';
  }

  for (version = 1; version < max_versions; ++version) {
    int fd;

    switch (clobber_policy) {
      case NMH_CLOBBER_AUTO: {
        snprintf (buffer, buflen, "%s-%ld%s%s", file, (long) version,
                  extension == NULL  ?  ""  :  ".",
                  extension == NULL  ?  ""  :  extension);
        break;
      }

      case NMH_CLOBBER_SUFFIX:
        snprintf (buffer, buflen, "%s.%ld", file, (long) version);
        break;

      default:
        /* Should never get here. */
        inform("will not overwrite %s, invalid clobber policy", buffer);
        free (buffer);
        return NULL;
    }

    /* Actually (try to) create the file here to avoid a race
       condition on file naming + creation.  This won't solve the
       problem with old NFS that doesn't support O_EXCL, though.
       Let the umask strip off permissions from 0666 as desired.
       That's what fopen () would do if it was creating the file. */
    if ((fd = open (buffer, O_CREAT | O_EXCL,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP |
                    S_IROTH | S_IWOTH)) >= 0) {
      close (fd);
      break;
    }
  }

  free (file);

  if (version >= max_versions) {
    inform("will not overwrite %s, too many versions", buffer);
    free (buffer);
    buffer = NULL;
  }

  return buffer;
}


static char *
clobber_check (char *original_file, mhstoreinfo_t info) {
  /* clobber policy        return value
   * --------------        ------------
   *   -always             original_file
   *   -auto               original_file-<digits>.extension
   *   -suffix             original_file.<digits>
   *   -ask                original_file, 0, or another filename/path
   *   -never              0
   */

  char *file;
  char *cwd = NULL;
  int check_again;

  if (! strcmp (original_file, "-")) {
      return original_file;
  }

  if (info->clobber_policy == NMH_CLOBBER_ASK) {
    /* Save cwd for possible use in loop below. */
    char *slash;

    cwd = mh_xstrdup(original_file);
    slash = strrchr (cwd, '/');

    if (slash) {
      *slash = '\0';
    } else {
      /* original_file isn't a full path, which should only happen if
         it is -. */
      free (cwd);
      cwd = NULL;
    }
  }

  do {
    struct stat st;

    file = original_file;
    check_again = 0;

    switch (info->clobber_policy) {
      case NMH_CLOBBER_ALWAYS:
        break;

      case NMH_CLOBBER_SUFFIX:
      case NMH_CLOBBER_AUTO:
        if (stat (file, &st) == OK) {
          if ((file = next_version (original_file, info->clobber_policy)) ==
              NULL) {
              ++info->files_not_clobbered;
          }
        }
        break;

      case NMH_CLOBBER_ASK:
        if (stat (file, &st) == OK) {
          enum answers { NMH_YES, NMH_NO, NMH_RENAME };
          static struct swit answer[4] = {
            { "yes", 0, NMH_YES },
            { "no", 0, NMH_NO },
            { "rename", 0, NMH_RENAME },
            { NULL, 0, 0 } };
          char **ans;

          if (isatty (fileno (stdin))) {
            char *prompt =
              concat ("Overwrite \"", file, "\" [y/n/rename]? ", NULL);
            ans = read_switch_multiword (prompt, answer);
            free (prompt);
          } else {
            /* Overwrite, that's what nmh used to do.  And warn. */
            inform("-clobber ask but no tty, so overwrite %s", file);
            break;
          }

          switch ((enum answers) smatch (*ans, answer)) {
            case NMH_YES:
              break;
            case NMH_NO:
              free (file);
              file = NULL;
              ++info->files_not_clobbered;
              break;
            case NMH_RENAME: {
              char buf[PATH_MAX];
              printf ("Enter filename or full path of the new file: ");
              if (fgets (buf, sizeof buf, stdin) == NULL  ||
                  buf[0] == '\0') {
                file = NULL;
                ++info->files_not_clobbered;
              } else {
                trim_suffix_c(buf, '\n');
              }

              free (file);

              if (buf[0] == '/') {
                /* Full path, use it. */
                file = mh_xstrdup(buf);
              } else {
                /* Relative path. */
                file = cwd  ?  concat (cwd, "/", buf, NULL)  :  mh_xstrdup(buf);
              }

              check_again = 1;
              break;
            }
          }
        }
        break;

      case NMH_CLOBBER_NEVER:
        if (stat (file, &st) == OK) {
          /* Keep count of files that would have been clobbered,
             and return that as process exit status. */
          inform("will not overwrite %s with -clobber never", file);
          free (file);
          file = NULL;
          ++info->files_not_clobbered;
        }
        break;
    }

    original_file = file;
  } while (check_again);

  free (cwd);

  return file;
}

static bool use_param_as_filename(const char *p)
{
    /* Preserve result of original test that considered an empty string
     * OK. */
    return !*p || (!strchr("/.|!", *p) && !strchr(p, '%'));
}

/* -clobber support */
/******************************************************************************/
