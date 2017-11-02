/* refile.c -- move or link message(s) from a source folder
 *          -- into one or more destination folders
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include "sbr/error.h"
#include "h/done.h"
#include <h/utils.h>
#include "sbr/m_maildir.h"
#include "sbr/m_mktemp.h"
#include <fcntl.h>

#define REFILE_SWITCHES \
    X("draft", 0, DRAFTSW) \
    X("link", 0, LINKSW) \
    X("nolink", 0, NLINKSW) \
    X("preserve", 0, PRESSW) \
    X("nopreserve", 0, NPRESSW) \
    X("retainsequences", 0, RETAINSEQSSW) \
    X("noretainsequences", 0, NRETAINSEQSSW) \
    X("unlink", 0, UNLINKSW) \
    X("nounlink", 0, NUNLINKSW) \
    X("src +folder", 0, SRCSW) \
    X("file file", 0, FILESW) \
    X("rmmproc program", 0, RPROCSW) \
    X("normmproc", 0, NRPRCSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(REFILE);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(REFILE, switches);
#undef X

static char maildir[BUFSIZ];

struct st_fold {
    char *f_name;
    struct msgs *f_mp;
};

/*
 * static prototypes
 */
static void opnfolds (struct msgs *, struct st_fold *, int);
static void clsfolds (struct st_fold *, int);
static void remove_files (int, char **);
static int m_file (struct msgs *, char *, int, struct st_fold *, int, int, int);
static void copy_seqs (struct msgs *, int, struct msgs *, int);


int
main (int argc, char **argv)
{
    bool linkf = false;
    bool preserve = false;
    bool retainseqs = false;
    int filep = 0;
    int foldp = 0, isdf = 0;
    bool unlink_msgs = false;
    int i, msgnum;
    char *cp, *folder = NULL, buf[BUFSIZ];
    char **argp, **arguments;
    char *filevec[NFOLDERS + 2];
    char **files = &filevec[1];	   /* leave room for remove_files:vec[0] */
    struct st_fold folders[NFOLDERS + 1];
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp;

    if (nmh_init(argv[0], true, true)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /*
     * Parse arguments
     */
    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
	    case AMBIGSW:
		ambigsw (cp, switches);
		done (1);
	    case UNKWNSW:
		die("-%s unknown\n", cp);

	    case HELPSW:
		snprintf (buf, sizeof(buf), "%s [msgs] [switches] +folder ...",
			  invo_name);
		print_help (buf, switches, 1);
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case LINKSW:
		linkf = true;
		continue;
	    case NLINKSW:
		linkf = false;
		continue;

	    case PRESSW:
		preserve = true;
		continue;
	    case NPRESSW:
		preserve = false;
		continue;

	    case RETAINSEQSSW:
		retainseqs = true;
		continue;
	    case NRETAINSEQSSW:
		retainseqs = false;
		continue;

	    case UNLINKSW:
		unlink_msgs = true;
		continue;
	    case NUNLINKSW:
		unlink_msgs = false;
		continue;

	    case SRCSW:
		if (folder)
		    die("only one source folder at a time!");
		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument to %s", argp[-2]);
		folder = path (*cp == '+' || *cp == '@' ? cp + 1 : cp,
			       *cp != '@' ? TFOLDER : TSUBCWF);
		continue;
	    case DRAFTSW:
		if (filep > NFOLDERS)
		    die("only %d files allowed!", NFOLDERS);
		isdf = 0;
		files[filep++] = mh_xstrdup(m_draft(NULL, NULL, 1, &isdf));
		continue;
	    case FILESW:
		if (filep > NFOLDERS)
		    die("only %d files allowed!", NFOLDERS);
		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument to %s", argp[-2]);
		files[filep++] = path (cp, TFILE);
		continue;

	    case RPROCSW:
		if (!(rmmproc = *argp++) || *rmmproc == '-')
		    die("missing argument to %s", argp[-2]);
		continue;
	    case NRPRCSW:
		rmmproc = NULL;
		continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (foldp > NFOLDERS)
		die("only %d folders allowed!", NFOLDERS);
	    folders[foldp++].f_name =
		pluspath (cp);
	} else
		app_msgarg(&msgs, cp);
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (foldp == 0)
	die("no folder specified");

    /*
     * We are refiling a file to the folders
     */
    if (filep > 0) {
	if (folder || msgs.size)
	    die("use -file or some messages, not both");
	opnfolds (NULL, folders, foldp);
	for (i = 0; i < filep; i++)
	    if (m_file (0, files[i], 0, folders, foldp, preserve, 0))
		done (1);
	/* If -nolink, then "remove" files */
	if (!linkf)
	    remove_files (filep, filevec);
	done (0);
    }

    if (!msgs.size)
	app_msgarg(&msgs, "cur");
    if (!folder)
	folder = getfolder (1);
    strncpy (maildir, m_maildir (folder), sizeof(maildir));

    if (chdir (maildir) == NOTOK)
	adios (maildir, "unable to change directory to");

    /* read source folder and create message structure */
    if (!(mp = folder_read (folder, 1)))
	die("unable to read folder %s", folder);

    /* check for empty folder */
    if (mp->nummsg == 0)
	die("no messages in %s", folder);

    /* parse the message range/sequence/name and set SELECTED */
    for (msgnum = 0; msgnum < msgs.size; msgnum++)
	if (!m_convert (mp, msgs.msgs[msgnum]))
	    done (1);
    seq_setprev (mp);	/* set the previous-sequence */

    /* create folder structures for each destination folder */
    opnfolds (mp, folders, foldp);

    /* Link all the selected messages into destination folders.
     *
     * This causes the add hook to be run for messages that are
     * linked into another folder.  The refile hook is run for
     * messages that are moved to another folder.
     */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (is_selected (mp, msgnum)) {
	    cp = mh_xstrdup(m_name (msgnum));
	    if (m_file (mp, cp, retainseqs ? msgnum : 0, folders, foldp,
                        preserve, !linkf))
		done (1);
	    free (cp);
	}
    }

    /*
     * Update this now, since folder_delmsgs() will save the context
     * but doesn't have access to the folder name.
     */

    context_replace (pfolder, folder);	/* update current folder   */

    /*
     * Adjust "cur" if necessary
     */

    if (mp->hghsel != mp->curmsg
	&& (mp->numsel != mp->nummsg || linkf))
	seq_setcur (mp, mp->hghsel);

    /*
     * Close destination folders now; if we are using private sequences
     * we need to have all of our calls to seq_save() complete before we
     * call context_save().
     */

    clsfolds (folders, foldp);

    /* If -nolink, then "remove" messages from source folder.
     *
     * Note that folder_delmsgs does not call the delete hook
     * because the message has already been handled above.
     */
    if (!linkf) {
	folder_delmsgs (mp, unlink_msgs, 1);
    } else {
	seq_save (mp);	/* synchronize message sequences */
	context_save ();			/* save the context file   */
    }

    folder_free (mp);			/* free folder structure   */
    done (0);
    return 1;
}


/*
 * Read all the destination folders and
 * create folder structures for all of them.
 */

static void
opnfolds (struct msgs *src_folder, struct st_fold *folders, int nfolders)
{
    char nmaildir[BUFSIZ];
    struct st_fold *fp, *ep;
    struct msgs *mp;

    for (fp = folders, ep = folders + nfolders; fp < ep; fp++) {
	if (chdir (m_maildir ("")) < 0) {
	    advise (m_maildir (""), "chdir");
	}
	strncpy (nmaildir, m_maildir (fp->f_name), sizeof(nmaildir));

	/*
	 * Null src_folder indicates that we are refiling a file to
	 * the folders, in which case we don't want to short-circuit
	 * fp->f_mp to any "source folder".
	 */
	if (! src_folder  ||  strcmp (src_folder->foldpath, nmaildir)) {
	    create_folder (nmaildir, 0, done);

	    if (chdir (nmaildir) == NOTOK)
		adios (nmaildir, "unable to change directory to");
	    if (!(mp = folder_read (fp->f_name, 1)))
		die("unable to read folder %s", fp->f_name);
	    mp->curmsg = 0;

	    fp->f_mp = mp;
	} else {
	    /* Source and destination folders are the same. */
	    fp->f_mp = src_folder;
	}

	if (maildir[0] != '\0'  &&  chdir (maildir) < 0) {
	    advise (maildir, "chdir");
	}
    }
}


/*
 * Set the Previous-Sequence and then synchronize the
 * sequence file, for each destination folder.
 */

static void
clsfolds (struct st_fold *folders, int nfolders)
{
    struct st_fold *fp, *ep;
    struct msgs *mp;

    for (fp = folders, ep = folders + nfolders; fp < ep; fp++) {
	mp = fp->f_mp;
	seq_setprev (mp);
	seq_save (mp);
    }
}


/*
 * If you have a "rmmproc" defined, we called that
 * to remove all the specified files.  If "rmmproc"
 * is not defined, then just unlink the files.
 */

static void
remove_files (int filep, char **files)
{
    int i, vecp;
    char **vec, *program;

    /* If rmmproc is defined, we use that */
    if (rmmproc) {
    	vec = argsplit(rmmproc, &program, &vecp);
	files++;		/* Yes, we need to do this */
	for (i = 0; i < filep; i++)
		vec[vecp++] = files[i];
	vec[vecp] = NULL;	/* NULL terminate list */

	fflush (stdout);
	execvp (program, vec);
	adios (rmmproc, "unable to exec");
    }

    /* Else just unlink the files */
    files++;	/* advance past filevec[0] */
    for (i = 0; i < filep; i++) {
	if (m_unlink (files[i]) == NOTOK)
	    admonish (files[i], "unable to unlink");
    }
}


/*
 * Link (or copy) the message into each of the destination folders.
 * If oldmsgnum is not 0, call copy_seqs().
 */

static int
m_file (struct msgs *mp, char *msgfile, int oldmsgnum,
	struct st_fold *folders, int nfolders, int preserve, int refile)
{
    int msgnum;
    struct st_fold *fp, *ep;

    for (fp = folders, ep = folders + nfolders; fp < ep; fp++) {
	/*
	 * With same source and destination folder, don't indicate that
	 * the new message is selected so that 1) folder_delmsgs() doesn't
	 * delete it later and 2) it is not reflected in mp->hghsel, and
	 * therefore won't be assigned to be the current message.
	 */
	if ((msgnum = folder_addmsg (&fp->f_mp, msgfile,
                                     mp != fp->f_mp,
				     0, preserve, nfolders == 1 && refile,
				     maildir)) == -1)
	    return 1;
	if (oldmsgnum) copy_seqs (mp, oldmsgnum, fp->f_mp, msgnum);
    }
    return 0;
}


/*
 * Copy sequence information for a refiled message to its
 * new folder.  Skip the cur sequence.
 */
static void
copy_seqs (struct msgs *oldmp, int oldmsgnum, struct msgs *newmp, int newmsgnum)
{
    char **seq;
    size_t seqnum;

    for (seq = svector_strs (oldmp->msgattrs), seqnum = 0;
	 *seq && seqnum < svector_size (oldmp->msgattrs);
	 ++seq, ++seqnum) {
	if (strcmp (current, *seq)) {
	    assert ((int) seqnum == seq_getnum (oldmp, *seq));
	    if (in_sequence (oldmp, seqnum, oldmsgnum)) {
		seq_addmsg (newmp, *seq, newmsgnum,
                            !is_seq_private (oldmp, seqnum), 0);
	    }
	}
    }
}
