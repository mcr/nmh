/*
 * flist.c -- list nmh folders containing messages
 *         -- in a given sequence
 *
 * originally by
 * David Nichols, Xerox-PARC, November, 1992
 *
 * Copyright (c) 1994 Xerox Corporation.
 * Use and copying of this software and preparation of derivative works based
 * upon this software are permitted. Any distribution of this software or
 * derivative works must comply with all applicable United States export
 * control laws. This software is made available AS IS, and Xerox Corporation
 * makes no warranty about the software, its performance or its conformity to
 * any specification.
 */

#include <h/mh.h>
#include <h/utils.h>

#define FALSE   0
#define TRUE    1

/*
 * We allocate space to record the names of folders
 * (foldersToDo array), this number of elements at a time.
 */
#define MAXFOLDERS  100


#define FLIST_SWITCHES \
    X("sequence name", 0, SEQSW) \
    X("all", 0, ALLSW) \
    X("noall", 0, NOALLSW) \
    X("recurse", 0, RECURSE) \
    X("norecurse", 0, NORECURSE) \
    X("showzero", 0, SHOWZERO) \
    X("noshowzero", 0, NOSHOWZERO) \
    X("alpha", 0, ALPHASW) \
    X("noalpha", 0, NOALPHASW) \
    X("fast", 0, FASTSW) \
    X("nofast", 0, NOFASTSW) \
    X("total", -5, TOTALSW) \
    X("nototal", -7, NOTOTALSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(FLIST);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(FLIST, switches);
#undef X

struct Folder {
    char *name;			/* name of folder */
    int priority;
    int error;			/* error == 1 for unreadable folder     */
    int nMsgs;			/* number of messages in folder         */
    int nSeq[NUMATTRS];		/* number of messages in each sequence  */
    int private[NUMATTRS];	/* is given sequence, public or private */
};

static struct Folder *orders = NULL;
static int nOrders = 0;
static int nOrdersAlloced = 0;
static struct Folder *folders = NULL;
static unsigned int nFolders = 0;
static int nFoldersAlloced = 0;

/* info on folders to search */
static char **foldersToDo;
static int numfolders;
static int maxfolders;

/* info on sequences to search for */
static char *sequencesToDo[NUMATTRS];
static unsigned int numsequences;

static int all        = FALSE;  /* scan all folders in top level?           */
static int alphaOrder = FALSE;	/* want alphabetical order only             */
static int recurse    = FALSE;	/* show nested folders?                     */
static int showzero   = TRUE;	/* show folders even if no messages in seq? */
static int Total      = TRUE;	/* display info on number of messages in    *
				 * sequence found, and total num messages   */

static char curfolder[BUFSIZ];	/* name of the current folder */
static char *nmhdir;		/* base nmh mail directory    */

/*
 * Type for a compare function for qsort.  This keeps
 * the compiler happy.
 */
typedef int (*qsort_comp) (const void *, const void *);

/*
 * prototypes
 */
int CompareFolders(struct Folder *, struct Folder *);
void GetFolderOrder(void);
void ScanFolders(void);
int AddFolder(char *, int);
void BuildFolderList(char *, int);
void BuildFolderListRecurse(char *, struct stat *, int);
void PrintFolders(void);
void AllocFolders(struct Folder **, int *, int);
int AssignPriority(char *);
static void do_readonly_folders(void);



int
main(int argc, char **argv)
{
    char *cp, **argp;
    char **arguments;
    char buf[BUFSIZ];

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex(argv[0], '/');

    /* read user profile/context */
    context_read();

    /*
     * If program was invoked with name ending
     * in `s', then add switch `-all'.
     */
    if (argv[0][strlen (argv[0]) - 1] == 's')
	all = TRUE;

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /* allocate the initial space to record the folder names */
    numfolders = 0;
    maxfolders = MAXFOLDERS;
    foldersToDo = (char **) mh_xmalloc ((size_t) (maxfolders * sizeof(*foldersToDo)));

    /* no sequences yet */
    numsequences = 0;

    /* parse arguments */
    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch(++cp, switches)) {
	    case AMBIGSW:
		ambigsw(cp, switches);
		done(1);
	    case UNKWNSW:
		adios(NULL, "-%s unknown", cp);

	    case HELPSW:
		snprintf(buf, sizeof(buf), "%s [+folder1 [+folder2 ...]][switches]",
			invo_name);
		print_help(buf, switches, 1);
		done(0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case SEQSW:
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);

		/* check if too many sequences specified */
		if (numsequences >= NUMATTRS)
		    adios (NULL, "too many sequences (more than %d) specified", NUMATTRS);
		sequencesToDo[numsequences++] = cp;
		break;

	    case ALLSW:
		all = TRUE;
		break;
	    case NOALLSW:
		all = FALSE;
		break;

	    case SHOWZERO:
		showzero = TRUE;
		break;
	    case NOSHOWZERO:
		showzero = FALSE;
		break;

	    case ALPHASW:
		alphaOrder = TRUE;
		break;
	    case NOALPHASW:
		alphaOrder = FALSE;
		break;

	    case NOFASTSW:
	    case TOTALSW:
		Total = TRUE;
		break;

	    case FASTSW:
	    case NOTOTALSW:
		Total = FALSE;
		break;

	    case RECURSE:
		recurse = TRUE;
		break;
	    case NORECURSE:
		recurse = FALSE;
		break;
	    }
	} else {
	    /*
	     * Check if we need to allocate more space
	     * for folder names.
	     */
	    if (numfolders >= maxfolders) {
		maxfolders += MAXFOLDERS;
		foldersToDo = (char **) mh_xrealloc (foldersToDo,
		    (size_t) (maxfolders * sizeof(*foldersToDo)));
	    }
	    if (*cp == '+' || *cp == '@') {
		foldersToDo[numfolders++] =
		    pluspath (cp);
	    } else
		foldersToDo[numfolders++] = cp;
	}
    }

    if (!context_find ("path"))
        free (path ("./", TFOLDER));

    /* get current folder */
    strncpy (curfolder, getfolder(1), sizeof(curfolder));

    /* get nmh base directory */
    nmhdir = m_maildir ("");

    /*
     * If we didn't specify any sequences, we search
     * for the "Unseen-Sequence" profile entry and use
     * all the sequences defined there.  We check to
     * make sure that the Unseen-Sequence entry doesn't
     * contain more than NUMATTRS sequences.
     */
    if (numsequences == 0) {
	if ((cp = context_find(usequence)) && *cp) {
	    char **ap, *dp;

	    dp = getcpy(cp);
	    ap = brkstring (dp, " ", "\n");
	    for (; ap && *ap; ap++) {
		if (numsequences >= NUMATTRS)
		    adios (NULL, "too many sequences (more than %d) in %s profile entry",
			   NUMATTRS, usequence);
		else
		    sequencesToDo[numsequences++] = *ap;
	    }
	} else {
	    adios (NULL, "no sequence specified or %s profile entry found", usequence);
	}
    }

    GetFolderOrder();
    ScanFolders();
    qsort(folders, nFolders, sizeof(struct Folder), (qsort_comp) CompareFolders);
    PrintFolders();
    done (0);
    return 1;
}

/*
 * Read the Flist-Order profile entry to determine
 * how to sort folders for output.
 */

void
GetFolderOrder(void)
{
    char *p, *s;
    int priority = 1;
    struct Folder *o;

    if (!(p = context_find("Flist-Order")))
	return;
    for (;;) {
	while (isspace((unsigned char) *p))
	    ++p;
	s = p;
	while (*p && !isspace((unsigned char) *p))
	    ++p;
	if (p != s) {
	    /* Found one. */
	    AllocFolders(&orders, &nOrdersAlloced, nOrders + 1);
	    o = &orders[nOrders++];
	    o->priority = priority++;
	    o->name = (char *) mh_xmalloc(p - s + 1);
	    strncpy(o->name, s, p - s);
	    o->name[p - s] = 0;
	} else
	    break;
    }
}

/*
 * Scan all the necessary folders
 */

void
ScanFolders(void)
{
    int i;

    /*
     * change directory to base of nmh directory
     */
    if (chdir (nmhdir) == NOTOK)
	adios (nmhdir, "unable to change directory to");

    if (numfolders > 0) {
	/* Update context */
	strncpy (curfolder, foldersToDo[numfolders - 1], sizeof(curfolder));
	context_replace (pfolder, curfolder);/* update current folder */
	context_save ();                     /* save the context file */

	/*
	 * Scan each given folder.  If -all is given,
	 * then also scan the 1st level subfolders under
	 * each given folder.
	 */
	for (i = 0; i < numfolders; ++i)
	    BuildFolderList(foldersToDo[i], all ? 1 : 0);
    } else {
	if (all) {
	    /*
	     * Do the readonly folders
	     */
	    do_readonly_folders();

	    /*
	     * Now scan the entire nmh directory for folders
	     */
	    BuildFolderList(".", 0);
	} else {
	    /*
	     * Else scan current folder
	     */
	    BuildFolderList(curfolder, 0);
	}
    }
}

/*
 * Initial building of folder list for
 * the top of our search tree.
 */

void
BuildFolderList(char *dirName, int searchdepth)
{
    struct stat st;

    /* Make sure we have a directory */
    if ((stat(dirName, &st) == -1) || !S_ISDIR(st.st_mode))
	return;

    /*
     * If base directory, don't add it to the
     * folder list. We just recurse into it.
     */
    if (!strcmp (dirName, ".")) {
	BuildFolderListRecurse (".", &st, 0);
	return;
    }

    /*
     * Add this folder to the list.
     * If recursing and directory has subfolders,
     * then build folder list for subfolders.
     */
    if (AddFolder(dirName, showzero) && (recurse || searchdepth) && st.st_nlink > 2)
	BuildFolderListRecurse(dirName, &st, searchdepth - 1);
}

/*
 * Recursive building of folder list
 */

void
BuildFolderListRecurse(char *dirName, struct stat *s, int searchdepth)
{
    char *base, name[PATH_MAX];
    char *n;
    int nlinks;
    DIR *dir;
    struct dirent *dp;
    struct stat st;

    /*
     * Keep track of number of directories we've seen so we can
     * stop stat'ing entries in this directory once we've seen
     * them all.  This optimization will fail if you have extra
     * directories beginning with ".", since we don't bother to
     * stat them.  But that shouldn't generally be a problem.
     */
    nlinks = s->st_nlink;
    if (nlinks == 1) {
      /* Disable the optimization under conditions where st_nlink
         is set to 1.  That happens on Cygwin, for example:
         http://cygwin.com/ml/cygwin-apps/2008-08/msg00264.html */
      nlinks = INT_MAX;
    }

    if (!(dir = opendir(dirName)))
	adios(dirName, "can't open directory");

    /*
     * A hack so that we don't see a
     * leading "./" in folder names.
     */
    base = strcmp (dirName, ".") ? dirName : dirName + 1;

    while (nlinks && (dp = readdir(dir))) {
	if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
	    nlinks--;
	    continue;
	}
	if (dp->d_name[0] == '.')
	    continue;
	/* Check to see if the name of the file is a number
	 * if it is, we assume it's a mail file and skip it
	 */
	for (n = dp->d_name; *n && isdigit((unsigned char) *n); n++);
	if (!*n)
	    continue;
	strncpy (name, base, sizeof(name) - 2);
	if (*base)
	    strcat(name, "/");
	strncat(name, dp->d_name, sizeof(name) - strlen(name) - 1);
	if ((stat(name, &st) != -1) && S_ISDIR(st.st_mode)) {
	    /*
	     * Check if this was really a symbolic link pointing
	     * to a directory.  If not, then decrement link count.
	     */
	    if (lstat (name, &st) == -1)
		nlinks--;
	    /* Add this folder to the list */
	    if (AddFolder(name, showzero) &&
			(recurse || searchdepth) && st.st_nlink > 2)
		BuildFolderListRecurse(name, &st, searchdepth - 1);
	}
    }
    closedir(dir);
}

/*
 * Add this folder to our list, counting the total number of
 * messages and the number of messages in each sequence.
 */

int
AddFolder(char *name, int force)
{
    unsigned int i;
    int msgnum, nonzero;
    int seqnum[NUMATTRS], nSeq[NUMATTRS];
    struct Folder *f;
    struct msgs *mp;

    /* Read folder and create message structure */
    if (!(mp = folder_read (name))) {
	/* Oops, error occurred.  Record it and continue. */
	AllocFolders(&folders, &nFoldersAlloced, nFolders + 1);
	f = &folders[nFolders++];
	f->name = getcpy(name);
	f->error = 1;
	f->priority = AssignPriority(f->name);
	return 0;
    }

    for (i = 0; i < numsequences; i++) {
	/* Convert sequences to their sequence numbers */
	if (sequencesToDo[i])
	    seqnum[i] = seq_getnum(mp, sequencesToDo[i]);
	else
	    seqnum[i] = -1;

	/* Now count messages in this sequence */
	nSeq[i] = 0;
	if (mp->nummsg > 0 && seqnum[i] != -1) {
	    for (msgnum = mp->lowmsg; msgnum <= mp->hghmsg; msgnum++) {
		if (in_sequence(mp, seqnum[i], msgnum))
		    nSeq[i]++;
	    }
	}
    }

    /* Check if any of the sequence checks were nonzero */
    nonzero = 0;
    for (i = 0; i < numsequences; i++) {
	if (nSeq[i] > 0) {
	    nonzero = 1;
	    break;
	}
    }

    if (nonzero || force) {
	/* save general folder information */
	AllocFolders(&folders, &nFoldersAlloced, nFolders + 1);
	f = &folders[nFolders++];
	f->name = getcpy(name);
	f->nMsgs = mp->nummsg;
	f->error = 0;
	f->priority = AssignPriority(f->name);

	/* record the sequence information */
	for (i = 0; i < numsequences; i++) {
	    f->nSeq[i] = nSeq[i];
	    f->private[i] = (seqnum[i] != -1) ? is_seq_private(mp, seqnum[i]) : 0;
	}
    }

    folder_free (mp);	/* free folder/message structure */
    return 1;
}

/*
 * Print the folder/sequence information
 */

void
PrintFolders(void)
{
    char tmpname[BUFSIZ];
    unsigned int i, j, len, has_private = 0;
    unsigned int maxfolderlen = 0, maxseqlen = 0;
    int maxnum = 0, maxseq = 0;

    if (!Total) {
	for (i = 0; i < nFolders; i++)
	    printf("%s\n", folders[i].name);
	return;
    }

    /*
     * Find the width we need for various fields
     */
    for (i = 0; i < nFolders; ++i) {
	/* find the length of longest folder name */
	len = strlen(folders[i].name);
	if (len > maxfolderlen)
	    maxfolderlen = len;

	/* If folder had error, skip the rest */
	if (folders[i].error)
	    continue;

	/* find the maximum total messages */
	if (folders[i].nMsgs > maxnum)
	    maxnum = folders[i].nMsgs;

	for (j = 0; j < numsequences; j++) {
	    /* find maximum width of sequence name */
	    len = strlen (sequencesToDo[j]);
	    if ((folders[i].nSeq[j] > 0 || showzero) && (len > maxseqlen))
		maxseqlen = len;

	    /* find the maximum number of messages in sequence */
	    if (folders[i].nSeq[j] > maxseq)
		maxseq = folders[i].nSeq[j];

	    /* check if this sequence is private in any of the folders */
	    if (folders[i].private[j])
		has_private = 1;
	}
    }

    /* Now print all the folder/sequence information */
    for (i = 0; i < nFolders; i++) {
	for (j = 0; j < numsequences; j++) {
	    if (folders[i].nSeq[j] > 0 || showzero) {
		/* Add `+' to end of name of current folder */
		if (strcmp(curfolder, folders[i].name))
		    snprintf(tmpname, sizeof(tmpname), "%s", folders[i].name);
		else
		    snprintf(tmpname, sizeof(tmpname), "%s+", folders[i].name);

		if (folders[i].error) {
		    printf("%-*s is unreadable\n", maxfolderlen+1, tmpname);
		    continue;
		}

		printf("%-*s has %*d in sequence %-*s%s; out of %*d\n",
		       maxfolderlen+1, tmpname,
		       num_digits(maxseq), folders[i].nSeq[j],
		       maxseqlen, sequencesToDo[j],
		       !has_private ? "" : folders[i].private[j] ? " (private)" : "          ",
		       num_digits(maxnum), folders[i].nMsgs);
	    }
	}
    }
}

/*
 * Put them in priority order.
 */

int
CompareFolders(struct Folder *f1, struct Folder *f2)
{
    if (!alphaOrder && f1->priority != f2->priority)
	return f1->priority - f2->priority;
    else
	return strcmp(f1->name, f2->name);
}

/*
 * Make sure we have at least n folders allocated.
 */

void
AllocFolders(struct Folder **f, int *nfa, int n)
{
    if (n <= *nfa)
	return;
    if (*f == NULL) {
	*nfa = 10;
	*f = (struct Folder *) mh_xmalloc (*nfa * (sizeof(struct Folder)));
    } else {
	*nfa *= 2;
	*f = (struct Folder *) mh_xrealloc (*f, *nfa * (sizeof(struct Folder)));
    }
}

/*
 * Return the priority for a name.  The highest comes from an exact match.
 * After that, the longest match (then first) assigns the priority.
 */
int
AssignPriority(char *name)
{
    int i, ol, nl;
    int best = nOrders;
    int bestLen = 0;
    struct Folder *o;

    nl = strlen(name);
    for (i = 0; i < nOrders; ++i) {
	o = &orders[i];
	if (!strcmp(name, o->name))
	    return o->priority;
	ol = strlen(o->name);
	if (nl < ol - 1)
	    continue;
	if (ol < bestLen)
	    continue;
	if (o->name[0] == '*' && !strcmp(o->name + 1, name + (nl - ol + 1))) {
	    best = o->priority;
	    bestLen = ol;
	} else if (o->name[ol - 1] == '*' && strncmp(o->name, name, ol - 1) == 0) {
	    best = o->priority;
	    bestLen = ol;
	}
    }
    return best;
}

/*
 * Do the read only folders
 */

static void
do_readonly_folders (void)
{
    int atrlen;
    char atrcur[BUFSIZ];
    register struct node *np;

    snprintf (atrcur, sizeof(atrcur), "atr-%s-", current);
    atrlen = strlen (atrcur);

    for (np = m_defs; np; np = np->n_next)
        if (ssequal (atrcur, np->n_name)
                && !ssequal (nmhdir, np->n_name + atrlen))
            BuildFolderList (np->n_name + atrlen, 0);
}
