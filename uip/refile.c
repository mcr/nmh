
/*
 * refile.c -- move or link message(s) from a source folder
 *          -- into one or more destination folders
 *
 * $Id$
 */

#include <h/mh.h>
#include <fcntl.h>
#include <errno.h>

/*
 * We allocate spaces for messages (msgs array)
 * this number of elements at a time.
 */
#define MAXMSGS  256


static struct swit switches[] = {
#define	DRAFTSW          0
    { "draft", 0 },
#define	LINKSW           1
    { "link", 0 },
#define	NLINKSW          2
    { "nolink", 0 },
#define	PRESSW           3
    { "preserve", 0 },
#define	NPRESSW          4
    { "nopreserve", 0 },
#define UNLINKSW         5
    { "unlink", 0 },
#define NUNLINKSW        6
    { "nounlink", 0 },
#define	SRCSW            7
    { "src +folder", 0 },
#define	FILESW           8
    { "file file", 0 },
#define	RPROCSW          9
    { "rmmproc program", 0 },
#define	NRPRCSW         10
    { "normmproc", 0 },
#define VERSIONSW       11
    { "version", 0 },
#define	HELPSW          12
    { "help", 4 },
    { NULL, 0 }
};

extern int errno;

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
static int m_file (char *, struct st_fold *, int, int);


int
main (int argc, char **argv)
{
    int	linkf = 0, preserve = 0, filep = 0;
    int foldp = 0, isdf = 0, unlink_msgs = 0;
    int i, msgnum, nummsgs, maxmsgs;
    char *cp, *folder = NULL, buf[BUFSIZ];
    char **argp, **arguments, **msgs;
    char *filevec[NFOLDERS + 2];
    char **files = &filevec[1];	   /* leave room for remove_files:vec[0] */
    struct st_fold folders[NFOLDERS + 1];
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
     * Allocate the initial space to record message
     * names, ranges, and sequences.
     */
    nummsgs = 0;
    maxmsgs = MAXMSGS;
    if (!(msgs = (char **) malloc ((size_t) (maxmsgs * sizeof(*msgs)))))
	adios (NULL, "unable to allocate storage");

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
		done (1);
	    case VERSIONSW:
		print_version(invo_name);
		done (1);

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
		path (cp + 1, *cp == '+' ? TFOLDER : TSUBCWF);
	} else {
	    /*
	     * Check if we need to allocate more space
	     * for message names, ranges, and sequences.
	     */
	    if (nummsgs >= maxmsgs) {
		maxmsgs += MAXMSGS;
		if (!(msgs = (char **) realloc (msgs,
						(size_t) (maxmsgs * sizeof(*msgs)))))
		    adios (NULL, "unable to reallocate msgs storage");
	    }
	    msgs[nummsgs++] = cp;
	}
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (foldp == 0)
	adios (NULL, "no folder specified");

#ifdef WHATNOW
    if (!nummsgs && !foldp && !filep && (cp = getenv ("mhdraft")) && *cp)
	files[filep++] = cp;
#endif /* WHATNOW */

    /*
     * We are refiling a file to the folders
     */
    if (filep > 0) {
	if (folder || nummsgs)
	    adios (NULL, "use -file or some messages, not both");
	opnfolds (folders, foldp);
	for (i = 0; i < filep; i++)
	    if (m_file (files[i], folders, foldp, preserve))
		done (1);
	/* If -nolink, then "remove" files */
	if (!linkf)
	    remove_files (filep, filevec);
	done (0);
    }

    if (!nummsgs)
	msgs[nummsgs++] = "cur";
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
    for (msgnum = 0; msgnum < nummsgs; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    done (1);
    seq_setprev (mp);	/* set the previous-sequence */

    /* create folder structures for each destination folder */
    opnfolds (folders, foldp);

    /* Link all the selected messages into destination folders */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (is_selected (mp, msgnum)) {
	    cp = getcpy (m_name (msgnum));
	    if (m_file (cp, folders, foldp, preserve))
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

    /* If -nolink, then "remove" messages from source folder */
    if (!linkf) {
	folder_delmsgs (mp, unlink_msgs);
    }

    clsfolds (folders, foldp);

    if (mp->hghsel != mp->curmsg
	&& (mp->numsel != mp->nummsg || linkf))
	seq_setcur (mp, mp->hghsel);
    seq_save (mp);	/* synchronize message sequences */

    context_replace (pfolder, folder);	/* update current folder   */
    context_save ();			/* save the context file   */
    folder_free (mp);			/* free folder structure   */
    return done (0);
}


/*
 * Read all the destination folders and
 * create folder structures for all of them.
 */

static void
opnfolds (struct st_fold *folders, int nfolders)
{
    register char *cp;
    char nmaildir[BUFSIZ];
    register struct st_fold *fp, *ep;
    register struct msgs *mp;
    struct stat st;

    for (fp = folders, ep = folders + nfolders; fp < ep; fp++) {
	chdir (m_maildir (""));
	strncpy (nmaildir, m_maildir (fp->f_name), sizeof(nmaildir));

	if (stat (nmaildir, &st) == NOTOK) {
	    if (errno != ENOENT)
		adios (nmaildir, "error on folder");
	    cp = concat ("Create folder \"", nmaildir, "\"? ", NULL);
	    if (!getanswer (cp))
		done (1);
	    free (cp);
	    if (!makedir (nmaildir))
		adios (NULL, "unable to create folder %s", nmaildir);
	}

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
    int i;
    char **vec;

    /* If rmmproc is defined, we use that */
    if (rmmproc) {
	vec = files++;		/* vec[0] = filevec[0] */
	files[filep] = NULL;	/* NULL terminate list */

	fflush (stdout);
	vec[0] = r1bindex (rmmproc, '/');
	execvp (rmmproc, vec);
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
 * Link (or copy) the message into each of
 * the destination folders.
 */

static int
m_file (char *msgfile, struct st_fold *folders, int nfolders, int preserve)
{
    int msgnum;
    struct st_fold *fp, *ep;

    for (fp = folders, ep = folders + nfolders; fp < ep; fp++) {
	if ((msgnum = folder_addmsg (&fp->f_mp, msgfile, 1, 0, preserve)) == -1)
	    return 1;
    }
    return 0;
}
