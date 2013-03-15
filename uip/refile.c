
/*
 * refile.c -- move or link message(s) from a source folder
 *          -- into one or more destination folders
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

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
static void opnfolds (struct st_fold *, int);
static void clsfolds (struct st_fold *, int);
static void remove_files (int, char **);
static int m_file (struct msgs *, char *, int, struct st_fold *, int, int, int);
static void copy_seqs (struct msgs *, int, struct msgs *, int);


int
main (int argc, char **argv)
{
    int	linkf = 0, preserve = 0, retainseqs = 0, filep = 0;
    int foldp = 0, isdf = 0, unlink_msgs = 0;
    int i, msgnum;
    char *cp, *folder = NULL, buf[BUFSIZ];
    char **argp, **arguments;
    char *filevec[NFOLDERS + 2];
    char **files = &filevec[1];	   /* leave room for remove_files:vec[0] */
    struct st_fold folders[NFOLDERS + 1];
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

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
		adios (NULL, "-%s unknown\n", cp);

	    case HELPSW:
		snprintf (buf, sizeof(buf), "%s [msgs] [switches] +folder ...",
			  invo_name);
		print_help (buf, switches, 1);
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case LINKSW:
		linkf++;
		continue;
	    case NLINKSW:
		linkf = 0;
		continue;

	    case PRESSW:
		preserve++;
		continue;
	    case NPRESSW:
		preserve = 0;
		continue;

	    case RETAINSEQSSW:
		retainseqs = 1;
		continue;
	    case NRETAINSEQSSW:
		retainseqs = 0;
		continue;

	    case UNLINKSW:
		unlink_msgs++;
		continue;
	    case NUNLINKSW:
		unlink_msgs = 0;
		continue;

	    case SRCSW:
		if (folder)
		    adios (NULL, "only one source folder at a time!");
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		folder = path (*cp == '+' || *cp == '@' ? cp + 1 : cp,
			       *cp != '@' ? TFOLDER : TSUBCWF);
		continue;
	    case DRAFTSW:
		if (filep > NFOLDERS)
		    adios (NULL, "only %d files allowed!", NFOLDERS);
		isdf = 0;
		files[filep++] = getcpy (m_draft (NULL, NULL, 1, &isdf));
		continue;
	    case FILESW:
		if (filep > NFOLDERS)
		    adios (NULL, "only %d files allowed!", NFOLDERS);
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		files[filep++] = path (cp, TFILE);
		continue;

	    case RPROCSW:
		if (!(rmmproc = *argp++) || *rmmproc == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;
	    case NRPRCSW:
		rmmproc = NULL;
		continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (foldp > NFOLDERS)
		adios (NULL, "only %d folders allowed!", NFOLDERS);
	    folders[foldp++].f_name =
		pluspath (cp);
	} else
		app_msgarg(&msgs, cp);
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (foldp == 0)
	adios (NULL, "no folder specified");

#ifdef WHATNOW
    if (!msgs.size && !foldp && !filep && (cp = getenv ("mhdraft")) && *cp)
	files[filep++] = cp;
#endif /* WHATNOW */

    /*
     * We are refiling a file to the folders
     */
    if (filep > 0) {
	if (folder || msgs.size)
	    adios (NULL, "use -file or some messages, not both");
	opnfolds (folders, foldp);
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
    if (!(mp = folder_read (folder)))
	adios (NULL, "unable to read folder %s", folder);

    /* check for empty folder */
    if (mp->nummsg == 0)
	adios (NULL, "no messages in %s", folder);

    /* parse the message range/sequence/name and set SELECTED */
    for (msgnum = 0; msgnum < msgs.size; msgnum++)
	if (!m_convert (mp, msgs.msgs[msgnum]))
	    done (1);
    seq_setprev (mp);	/* set the previous-sequence */

    /* create folder structures for each destination folder */
    opnfolds (folders, foldp);

    /* Link all the selected messages into destination folders.
     *
     * This causes the add hook to be run for messages that are
     * linked into another folder.  The refile hook is run for
     * messages that are moved to another folder.
     */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (is_selected (mp, msgnum)) {
	    cp = getcpy (m_name (msgnum));
	    if (m_file (mp, cp, retainseqs ? msgnum : 0, folders, foldp,
                        preserve, !linkf))
		done (1);
	    free (cp);
	}
    }

    /*
     * This is a hack.  If we are using an external rmmproc,
     * then save the current folder to the context file,
     * so the external rmmproc will remove files from the correct
     * directory.  This should be moved to folder_delmsgs().
     */
    if (rmmproc) {
	context_replace (pfolder, folder);
	context_save ();
	fflush (stdout);
    }

    /* If -nolink, then "remove" messages from source folder.
     *
     * Note that folder_delmsgs does not call the delete hook
     * because the message has already been handled above.
     */
    if (!linkf) {
	folder_delmsgs (mp, unlink_msgs, 1);
    }

    clsfolds (folders, foldp);

    if (mp->hghsel != mp->curmsg
	&& (mp->numsel != mp->nummsg || linkf))
	seq_setcur (mp, mp->hghsel);
    seq_save (mp);	/* synchronize message sequences */

    context_replace (pfolder, folder);	/* update current folder   */
    context_save ();			/* save the context file   */
    folder_free (mp);			/* free folder structure   */
    done (0);
    return 1;
}


/*
 * Read all the destination folders and
 * create folder structures for all of them.
 */

static void
opnfolds (struct st_fold *folders, int nfolders)
{
    char nmaildir[BUFSIZ];
    register struct st_fold *fp, *ep;
    register struct msgs *mp;

    for (fp = folders, ep = folders + nfolders; fp < ep; fp++) {
	chdir (m_maildir (""));
	strncpy (nmaildir, m_maildir (fp->f_name), sizeof(nmaildir));

    create_folder (nmaildir, 0, done);

	if (chdir (nmaildir) == NOTOK)
	    adios (nmaildir, "unable to change directory to");
	if (!(mp = folder_read (fp->f_name)))
	    adios (NULL, "unable to read folder %s", fp->f_name);
	mp->curmsg = 0;

	fp->f_mp = mp;

	chdir (maildir);
    }
}


/*
 * Set the Previous-Sequence and then sychronize the
 * sequence file, for each destination folder.
 */

static void
clsfolds (struct st_fold *folders, int nfolders)
{
    register struct st_fold *fp, *ep;
    register struct msgs *mp;

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
	if (unlink (files[i]) == NOTOK)
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
	if ((msgnum = folder_addmsg (&fp->f_mp, msgfile, 1, 0, preserve, nfolders == 1 && refile, maildir)) == -1)
	    return 1;
	if (oldmsgnum) copy_seqs (mp, oldmsgnum, fp->f_mp, msgnum);
    }
    return 0;
}


/*
 * Copy sequence information for a refiled message to its
 * new folder(s).  Skip the cur sequence.
 */
static void
copy_seqs (struct msgs *oldmp, int oldmsgnum, struct msgs *newmp, int newmsgnum)
{
    char **seq;
    int seqnum;

    for (seq = oldmp->msgattrs, seqnum = 0; *seq; ++seq, ++seqnum) {
	if (strcmp (current, *seq)) {
	    assert (seqnum == seq_getnum (oldmp, *seq));
	    if (in_sequence (oldmp, seqnum, oldmsgnum)) {
		seq_addmsg (newmp, *seq, newmsgnum,
			    is_seq_private (oldmp, seqnum) ? 0 : 1, 0);
	    }
	}
    }
}
