
/*
 * folder(s).c -- set/list the current message and/or folder
 *             -- push/pop a folder onto/from the folder stack
 *             -- list the folder stack
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <errno.h>

static struct swit switches[] = {
#define	ALLSW           0
    { "all", 0 },
#define NALLSW		1
    { "noall", 0 },
#define	CREATSW         2
    { "create", 0 },
#define	NCREATSW        3
    { "nocreate", 0 },
#define	FASTSW          4
    { "fast", 0 },
#define	NFASTSW         5
    { "nofast", 0 },
#define	HDRSW           6
    { "header", 0 },
#define	NHDRSW          7
    { "noheader", 0 },
#define	PACKSW          8
    { "pack", 0 },
#define	NPACKSW         9
    { "nopack", 0 },
#define	VERBSW         10
    { "verbose", 0 },
#define	NVERBSW        11
    { "noverbose", 0 },
#define	RECURSW        12
    { "recurse", 0 },
#define	NRECRSW        13
    { "norecurse", 0 },
#define	TOTALSW        14
    { "total", 0 },
#define	NTOTLSW        15
    { "nototal", 0 },
#define	LISTSW         16
    { "list", 0 },
#define	NLISTSW        17
    { "nolist", 0 },
#define	PRNTSW         18
    { "print", 0 },
#define	NPRNTSW        19
    { "noprint", -4 },
#define	PUSHSW         20
    { "push", 0 },
#define	POPSW          21
    { "pop", 0 },
#define VERSIONSW      22
    { "version", 0 },
#define	HELPSW         23
    { "help", 0 },
    { NULL, 0 }
};

static int fshort   = 0;	/* output only folder names                 */
static int fcreat   = 0;	/* should we ask to create new folders?     */
static int fpack    = 0;	/* are we packing the folder?               */
static int fverb    = 0;	/* print actions taken while packing folder */
static int fheader  = 0;	/* should we output a header?               */
static int frecurse = 0;	/* recurse through subfolders               */
static int ftotal   = 0;	/* should we output the totals?             */
static int all      = 0;	/* should we output all folders             */

static int total_folders = 0;	/* total number of folders                  */

static int start = 0;
static int foldp = 0;

static char *nmhdir;
static char *stack = "Folder-Stack";
static char folder[BUFSIZ];

#define NUMFOLDERS 100

/*
 * This is how many folders we currently can hold in the array
 * `folds'.  We increase this amount by NUMFOLDERS at a time.
 */
static int maxfolders;
static char **folds;

/*
 * Structure to hold information about
 * folders as we scan them.
 */
struct FolderInfo {
    char *name;
    int nummsg;
    int curmsg;
    int lowmsg;
    int hghmsg;
    int others;		/* others == 1 if other files in folder */
    int error;		/* error == 1 for unreadable folder     */
};

/*
 * Dynamically allocated space to hold
 * all the folder information.
 */
static struct FolderInfo *fi;
static int maxFolderInfo;

/*
 * static prototypes
 */
static void dodir (char *);
static int get_folder_info (char *, char *);
static void print_folders (void);
static int num_digits (int);
static int sfold (struct msgs *, char *);
static void addir (char *);
static void addfold (char *);
static int compare (char *, char *);
static void readonly_folders (void);


int
main (int argc, char **argv)
{
    int printsw = 0, listsw = 0;
    int pushsw = 0, popsw = 0;
    char *cp, *dp, *msg = NULL, *argfolder = NULL;
    char **ap, **argp, buf[BUFSIZ], **arguments;
    struct stat st;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    /*
     * If program was invoked with name ending
     * in `s', then add switch `-all'.
     */
    if (argv[0][strlen (argv[0]) - 1] == 's')
	all = 1;

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [+folder] [msg] [switches]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case ALLSW: 
		    all = 1;
		    continue;

		case NALLSW:
		    all = 0;
		    continue;

		case CREATSW: 
		    fcreat = 1;
		    continue;
		case NCREATSW: 
		    fcreat = -1;
		    continue;

		case FASTSW: 
		    fshort++;
		    continue;
		case NFASTSW: 
		    fshort = 0;
		    continue;

		case HDRSW: 
		    fheader = 1;
		    continue;
		case NHDRSW: 
		    fheader = -1;
		    continue;

		case PACKSW: 
		    fpack++;
		    continue;
		case NPACKSW: 
		    fpack = 0;
		    continue;

		case VERBSW:
		    fverb++;
		    continue;
		case NVERBSW:
		    fverb = 0;
		    continue;

		case RECURSW: 
		    frecurse++;
		    continue;
		case NRECRSW: 
		    frecurse = 0;
		    continue;

		case TOTALSW: 
		    ftotal = 1;
		    continue;
		case NTOTLSW: 
		    ftotal = -1;
		    continue;

		case PRNTSW: 
		    printsw = 1;
		    continue;
		case NPRNTSW: 
		    printsw = 0;
		    continue;

		case LISTSW: 
		    listsw = 1;
		    continue;
		case NLISTSW: 
		    listsw = 0;
		    continue;

		case PUSHSW: 
		    pushsw = 1;
		    listsw = 1;
		    popsw  = 0;
		    continue;
		case POPSW: 
		    popsw  = 1;
		    listsw = 1;
		    pushsw = 0;
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (argfolder)
		adios (NULL, "only one folder at a time!");
	    else
		argfolder = path (cp + 1, *cp == '+' ? TFOLDER : TSUBCWF);
	} else {
	    if (msg)
		adios (NULL, "only one (current) message at a time!");
	    else
		msg = cp;
	}
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    nmhdir = concat (m_maildir (""), "/", NULL);

    /*
     * If we aren't working with the folder stack
     * (-push, -pop, -list) then the default is to print.
     */
    if (pushsw == 0 && popsw == 0 && listsw == 0)
	printsw++;

    /* Pushing a folder onto the folder stack */
    if (pushsw) {
	if (!argfolder) {
	    /* If no folder is given, the current folder and */
	    /* the top of the folder stack are swapped.      */
	    if ((cp = context_find (stack))) {
		dp = getcpy (cp);
		ap = brkstring (dp, " ", "\n");
		argfolder = getcpy(*ap++);
	    } else {
		adios (NULL, "no other folder");
	    }
	    for (cp = getcpy (getfolder (1)); *ap; ap++)
		cp = add (*ap, add (" ", cp));
	    free (dp);
	    context_replace (stack, cp);	/* update folder stack */
	} else {
	    /* update folder stack */
	    context_replace (stack,
		    (cp = context_find (stack))
		    ? concat (getfolder (1), " ", cp, NULL)
		    : getcpy (getfolder (1)));
	}
    }

    /* Popping a folder off of the folder stack */
    if (popsw) {
	if (argfolder)
	    adios (NULL, "sorry, no folders allowed with -pop");
	if ((cp = context_find (stack))) {
	    dp = getcpy (cp);
	    ap = brkstring (dp, " ", "\n");
	    argfolder = getcpy(*ap++);
	} else {
	    adios (NULL, "folder stack empty");
	}
	if (*ap) {
	    /* if there's anything left in the stack */
	    cp = getcpy (*ap++);
	    for (; *ap; ap++)
		cp = add (*ap, add (" ", cp));
	    context_replace (stack, cp);	/* update folder stack */
	} else {
	    context_del (stack);	/* delete folder stack entry from context */
	}
	free (dp);
    }
    if (pushsw || popsw) {
	cp = m_maildir(argfolder);
	if (access (cp, F_OK) == NOTOK)
	    adios (cp, "unable to find folder");
	context_replace (pfolder, argfolder);	/* update current folder   */
	context_save ();		/* save the context file   */
	argfolder = NULL;
    }

    /* Listing the folder stack */
    if (listsw) {
	printf ("%s", argfolder ? argfolder : getfolder (1));
	if ((cp = context_find (stack))) {
	    dp = getcpy (cp);
	    for (ap = brkstring (dp, " ", "\n"); *ap; ap++)
		printf (" %s", *ap);
	    free (dp);
	}
	printf ("\n");

	if (!printsw)
	    done (0);
    }

    /* Allocate initial space to record folder names */
    maxfolders = NUMFOLDERS;
    if ((folds = malloc (maxfolders * sizeof(char *))) == NULL)
	adios (NULL, "unable to allocate storage for folder names");

    /* Allocate initial space to record folder information */
    maxFolderInfo = NUMFOLDERS;
    if ((fi = malloc (maxFolderInfo * sizeof(*fi))) == NULL)
	adios (NULL, "unable to allocate storage for folder info");

    /*
     * Scan the folders
     */
    if (all || ftotal > 0) {
	/*
	 * If no folder is given, do them all
	 */
	if (!argfolder) {
	    if (msg)
		admonish (NULL, "no folder given for message %s", msg);
	    readonly_folders (); /* do any readonly folders */
	    strncpy (folder, (cp = context_find (pfolder)) ? cp : "", sizeof(folder));
	    dodir (".");
	} else {
	    strncpy (folder, argfolder, sizeof(folder));
	    if (get_folder_info (argfolder, msg)) {
		context_replace (pfolder, argfolder);/* update current folder */
		context_save ();		     /* save the context file */
	    }
	    /*
	     * Since recurse wasn't done in get_folder_info(),
	     * we still need to list all level-1 sub-folders.
	     */
	    if (!frecurse)
		dodir (folder);
	}
    } else {
	strncpy (folder, argfolder ? argfolder : getfolder (1), sizeof(folder));

	/*
	 * Check if folder exists.  If not, then see if
	 * we should create it, or just exit.
	 */
	if (stat (strncpy (buf, m_maildir (folder), sizeof(buf)), &st) == -1) {
	    if (errno != ENOENT)
		adios (buf, "error on folder");
	    if (fcreat == 0) {
		/* ask before creating folder */
		cp = concat ("Create folder \"", buf, "\"? ", NULL);
		if (!getanswer (cp))
		    done (1);
		free (cp);
	    } else if (fcreat == -1) {
		/* do not create, so exit */
		done (1);
	    }
	    if (!makedir (buf))
		adios (NULL, "unable to create folder %s", buf);
	}

	if (get_folder_info (folder, msg) && argfolder) {
	    /* update current folder */
	    context_replace (pfolder, argfolder);
	    }
    }

    /*
     * Print out folder information
     */
    print_folders();

    context_save ();	/* save the context file */
    return done (0);
}

/*
 * Base routine for scanning a folder
 */

static void
dodir (char *dir)
{
    int i;
    int os = start;
    int of = foldp;
    char buffer[BUFSIZ];

    start = foldp;

    /* change directory to base of nmh directory */
    if (chdir (nmhdir) == NOTOK)
	adios (nmhdir, "unable to change directory to");

    addir (strncpy (buffer, dir, sizeof(buffer)));

    for (i = start; i < foldp; i++) {
	get_folder_info (folds[i], NULL);
	fflush (stdout);
    }

    start = os;
    foldp = of;
}

static int
get_folder_info (char *fold, char *msg)
{
    int	i, retval = 1;
    char *mailfile;
    struct msgs *mp = NULL;

    i = total_folders++;

    /*
     * if necessary, reallocate the space
     * for folder information
     */
    if (total_folders >= maxFolderInfo) {
	maxFolderInfo += NUMFOLDERS;
	if ((fi = realloc (fi, maxFolderInfo * sizeof(*fi))) == NULL)
	    adios (NULL, "unable to re-allocate storage for folder info");
    }

    fi[i].name   = fold;
    fi[i].nummsg = 0;
    fi[i].curmsg = 0;
    fi[i].lowmsg = 0;
    fi[i].hghmsg = 0;
    fi[i].others = 0;
    fi[i].error  = 0;

    mailfile = m_maildir (fold);

    if (!chdir (mailfile)) {
	if ((ftotal > 0) || !fshort || msg || fpack) {
	    /*
	     * create message structure and get folder info
	     */
	    if (!(mp = folder_read (fold))) {
		admonish (NULL, "unable to read folder %s", fold);
		return 0;
	    }

	    /* set the current message */
	    if (msg && !sfold (mp, msg))
		retval = 0;

	    if (fpack) {
		if (folder_pack (&mp, fverb) == -1)
		    done (1);
		seq_save (mp);		/* synchronize the sequences */
		context_save ();	/* save the context file     */
	    }

	    /* record info for this folder */
	    if ((ftotal > 0) || !fshort) {
		fi[i].nummsg = mp->nummsg;
		fi[i].curmsg = mp->curmsg;
		fi[i].lowmsg = mp->lowmsg;
		fi[i].hghmsg = mp->hghmsg;
		fi[i].others = other_files (mp);
	    }

	    folder_free (mp); /* free folder/message structure */
	}
    } else {
	fi[i].error = 1;
    }

    if (frecurse && (fshort || fi[i].others) && (fi[i].error == 0))
	dodir (fold);
    return retval;
}

/*
 * Print folder information
 */

static void
print_folders (void)
{
    int i, len, hasempty = 0, curprinted;
    int maxlen = 0, maxnummsg = 0, maxlowmsg = 0;
    int maxhghmsg = 0, maxcurmsg = 0, total_msgs = 0;
    int nummsgdigits, lowmsgdigits;
    int hghmsgdigits, curmsgdigits;
    char tmpname[BUFSIZ];

    /*
     * compute a few values needed to for
     * printing various fields
     */
    for (i = 0; i < total_folders; i++) {
	/* length of folder name */
	len = strlen (fi[i].name);
	if (len > maxlen)
	    maxlen = len;

	/* If folder has error, skip the rest */
	if (fi[i].error)
	    continue;

	/* calculate total number of messages */
	total_msgs += fi[i].nummsg;

	/* maximum number of messages */
	if (fi[i].nummsg > maxnummsg)
	    maxnummsg = fi[i].nummsg;

	/* maximum low message */
	if (fi[i].lowmsg > maxlowmsg)
	    maxlowmsg = fi[i].lowmsg;

	/* maximum high message */
	if (fi[i].hghmsg > maxhghmsg)
	    maxhghmsg = fi[i].hghmsg;

	/* maximum current message */
	if (fi[i].curmsg >= fi[i].lowmsg &&
	    fi[i].curmsg <= fi[i].hghmsg &&
	    fi[i].curmsg > maxcurmsg)
	    maxcurmsg = fi[i].curmsg;

	/* check for empty folders */
	if (fi[i].nummsg == 0)
	    hasempty = 1;
    }
    nummsgdigits = num_digits (maxnummsg);
    lowmsgdigits = num_digits (maxlowmsg);
    hghmsgdigits = num_digits (maxhghmsg);
    curmsgdigits = num_digits (maxcurmsg);

    if (hasempty && nummsgdigits < 2)
	nummsgdigits = 2;

    /*
     * Print the header
     */
    if (fheader > 0 || (all && !fshort && fheader >= 0))
	printf ("%-*s %*s %-*s; %-*s %*s\n",
		maxlen+1, "FOLDER",
		nummsgdigits + 13, "# MESSAGES",
		lowmsgdigits + hghmsgdigits + 4, " RANGE",
		curmsgdigits + 4, "CUR",
		9, "(OTHERS)");

    /*
     * Print folder information
     */
    if (all || fshort || ftotal < 1) {
	for (i = 0; i < total_folders; i++) {
	    if (fshort) {
		printf ("%s\n", fi[i].name);
		continue;
	    }

	    /* Add `+' to end of name, if folder is current */
	    if (strcmp (folder, fi[i].name))
		snprintf (tmpname, sizeof(tmpname), "%s", fi[i].name);
	    else
		snprintf (tmpname, sizeof(tmpname), "%s+", fi[i].name);

	    if (fi[i].error) {
		printf ("%-*s is unreadable\n", maxlen+1, tmpname);
		continue;
	    }

	    printf ("%-*s ", maxlen+1, tmpname);

	    curprinted = 0; /* remember if we print cur */
	    if (fi[i].nummsg == 0) {
		printf ("has %*s messages%*s",
			nummsgdigits, "no",
			fi[i].others ? lowmsgdigits + hghmsgdigits + 5 : 0, "");
	    } else {
		printf ("has %*d message%s  (%*d-%*d)",
			nummsgdigits, fi[i].nummsg,
			(fi[i].nummsg == 1) ? " " : "s",
			lowmsgdigits, fi[i].lowmsg,
			hghmsgdigits, fi[i].hghmsg);
		if (fi[i].curmsg >= fi[i].lowmsg && fi[i].curmsg <= fi[i].hghmsg) {
		    curprinted = 1;
		    printf ("; cur=%*d", curmsgdigits, fi[i].curmsg);
		}
	    }

	    if (fi[i].others)
		printf (";%*s (others)", curprinted ? 0 : curmsgdigits + 6, "");
	    printf (".\n");
	}
    }

    /*
     * Print folder/message totals
     */
    if (ftotal > 0 || (all && !fshort && ftotal >= 0)) {
	if (all)
	    printf ("\n");
	printf ("TOTAL = %d message%c in %d folder%s.\n",
		total_msgs, total_msgs != 1 ? 's' : ' ',
		total_folders, total_folders != 1 ? "s" : "");
    }

    fflush (stdout);
}

/*
 * Calculate the number of digits in a nonnegative integer
 */
int
num_digits (int n)
{
    int ndigits = 0;

    /* Sanity check */
    if (n < 0)
	adios (NULL, "oops, num_digits called with negative value");

    if (n == 0)
	return 1;

    while (n) {
	n /= 10;
	ndigits++;
    }

    return ndigits;
}

/*
 * Set the current message and sychronize sequences
 */

static int
sfold (struct msgs *mp, char *msg)
{
    /* parse the message range/sequence/name and set SELECTED */
    if (!m_convert (mp, msg))
	return 0;

    if (mp->numsel > 1) {
	admonish (NULL, "only one message at a time!");
	return 0;
    }
    seq_setprev (mp);		/* set the previous-sequence     */
    seq_setcur (mp, mp->lowsel);/* set current message           */
    seq_save (mp);		/* synchronize message sequences */
    context_save ();		/* save the context file         */

    return 1;
}


static void
addir (char *name)
{
    int nlink;
    char *base, *cp;
    struct stat st;
    struct dirent *dp;
    DIR * dd;

    cp = name + strlen (name);
    *cp++ = '/';
    *cp = '\0';

    /*
     * A hack to skip over a leading
     * "./" in folder names.
     */
    base = strcmp (name, "./") ? name : name + 2;

   /* short-cut to see if directory has any sub-directories */
    if (stat (name, &st) != -1 && st.st_nlink == 2)
        return;
 
    if (!(dd = opendir (name))) {
	admonish (name, "unable to read directory ");
	return;
    }

    /*
     * Keep track of the number of directories we've seen
     * so we can quit stat'ing early, if we've seen them all.
     */
    nlink = st.st_nlink;

    while (nlink && (dp = readdir (dd))) {
	if (!strcmp (dp->d_name, ".") || !strcmp (dp->d_name, "..")) {
	    nlink--;
	    continue;
	}
	if (cp + NLENGTH(dp) + 2 >= name + BUFSIZ)
	    continue;
	strcpy (cp, dp->d_name);
	if (stat (name, &st) != -1 && S_ISDIR(st.st_mode)) {
	    /*
	     * Check if this was really a symbolic link pointing at
	     * a directory.  If not, then decrement link count.
	     */
	    if (lstat (name, &st) == -1)
		nlink--;
	    addfold (base);
	}
    }

    closedir (dd);
    *--cp = '\0';
}

/*
 * Add the folder name into the
 * list in a sorted fashion.
 */

static void
addfold (char *fold)
{
    register int i, j;
    register char *cp;

    /* if necessary, reallocate the space for folder names */
    if (foldp >= maxfolders) {
	maxfolders += NUMFOLDERS;
	if ((folds = realloc (folds, maxfolders * sizeof(char *))) == NULL)
	    adios (NULL, "unable to re-allocate storage for folder names");
    }

    cp = getcpy (fold);
    for (i = start; i < foldp; i++)
	if (compare (cp, folds[i]) < 0) {
	    for (j = foldp - 1; j >= i; j--)
		folds[j + 1] = folds[j];
	    foldp++;
	    folds[i] = cp;
	    return;
	}

    folds[foldp++] = cp;
}


static int
compare (char *s1, char *s2)
{
    register int i;

    while (*s1 || *s2)
	if ((i = *s1++ - *s2++))
	    return i;

    return 0;
}

/*
 * Do the read only folders
 */

static void
readonly_folders (void)
{
    int	atrlen;
    char atrcur[BUFSIZ];
    register struct node *np;

    snprintf (atrcur, sizeof(atrcur), "atr-%s-", current);
    atrlen = strlen (atrcur);

    for (np = m_defs; np; np = np->n_next)
	if (ssequal (atrcur, np->n_name)
		&& !ssequal (nmhdir, np->n_name + atrlen))
	    get_folder_info (np->n_name + atrlen, NULL);
}
