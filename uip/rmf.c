
/*
 * rmf.c -- remove a folder
 *
 * $Id$
 */

#include <h/mh.h>

static struct swit switches[] = {
#define	INTRSW            0
    { "interactive", 0 },
#define	NINTRSW           1
    { "nointeractive", 0 },
#define VERSIONSW         2
    { "version", 0 },
#define	HELPSW            3
    { "help", 4 },
    { NULL, 0 }
};

/*
 * static prototypes
 */
static int rmf(char *);
static void rma (char *);


int
main (int argc, char **argv)
{
    int defolder = 0, interactive = -1;
    char *cp, *folder = NULL, newfolder[BUFSIZ];
    char buf[BUFSIZ], **argp, **arguments;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

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
		    snprintf (buf, sizeof(buf), "%s [+folder] [switches]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case INTRSW: 
		    interactive = 1;
		    continue;
		case NINTRSW: 
		    interactive = 0;
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		adios (NULL, "only one folder at a time!");
	    else
		folder = path (cp + 1, *cp == '+' ? TFOLDER : TSUBCWF);
	} else {
	    adios (NULL, "usage: %s [+folder] [switches]", invo_name);
	}
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (!folder) {
	folder = getfolder (1);
	defolder++;
    }
    if (strcmp (m_mailpath (folder), pwd ()) == 0)
	adios (NULL, "sorry, you can't remove the current working directory");

    if (interactive == -1)
	interactive = defolder;

    if (strchr (folder, '/') && (*folder != '/') && (*folder != '.')) {
	for (cp = copy (folder, newfolder); cp > newfolder && *cp != '/'; cp--)
	    continue;
	if (cp > newfolder)
	    *cp = '\0';
	else
	    strncpy (newfolder, getfolder(0), sizeof(newfolder));
    } else {
	strncpy (newfolder, getfolder(0), sizeof(newfolder));
    }

    if (interactive) {
	cp = concat ("Remove folder \"", folder, "\"? ", NULL);
	if (!getanswer (cp))
	    done (0);
	free (cp);
    }

    if (rmf (folder) == OK && strcmp (context_find (pfolder), newfolder)) {
	printf ("[+%s now current]\n", newfolder);
	context_replace (pfolder, newfolder);	/* update current folder */
    }
    context_save ();	/* save the context file */
    done (0);
}

static int
rmf (char *folder)
{
    int i, j, others;
    register char *maildir;
    char cur[BUFSIZ];
    register struct dirent *dp;
    register DIR *dd;

    switch (i = chdir (maildir = m_maildir (folder))) {
	case OK: 
	    if (access (".", W_OK) != NOTOK && access ("..", W_OK) != NOTOK)
		break;		/* fall otherwise */

	case NOTOK: 
	    snprintf (cur, sizeof(cur), "atr-%s-%s",
			current, m_mailpath (folder));
	    if (!context_del (cur)) {
		printf ("[+%s de-referenced]\n", folder);
		return OK;
	    }
	    advise (NULL, "you have no profile entry for the %s folder +%s",
		    i == NOTOK ? "unreadable" : "read-only", folder);
	    return NOTOK;
    }

    if ((dd = opendir (".")) == NULL)
	adios (NULL, "unable to read folder +%s", folder);
    others = 0;

    j = strlen(BACKUP_PREFIX);
    while ((dp = readdir (dd))) {
	switch (dp->d_name[0]) {
	    case '.': 
		if (strcmp (dp->d_name, ".") == 0
			|| strcmp (dp->d_name, "..") == 0)
		    continue;	/* else fall */

	    case ',': 
#ifdef MHE
	    case '+': 
#endif /* MHE */
#ifdef UCI
	    case '_': 
	    case '#': 
#endif /* UCI */
		break;

	    default: 
		if (m_atoi (dp->d_name))
		    break;
		if (strcmp (dp->d_name, LINK) == 0
			|| strncmp (dp->d_name, BACKUP_PREFIX, j) == 0)
		    break;

		admonish (NULL, "file \"%s/%s\" not deleted",
			folder, dp->d_name);
		others++;
		continue;
	}
	if (unlink (dp->d_name) == NOTOK) {
	    admonish (dp->d_name, "unable to unlink %s:", folder);
	    others++;
	}
    }

    closedir (dd);

    /*
     * Remove any relevant private sequences
     * or attributes from context file.
     */
    rma (folder);

    chdir ("..");
    if (others == 0 && remdir (maildir))
	return OK;

    advise (NULL, "folder +%s not removed", folder);
    return NOTOK;
}


/*
 * Remove all the (private) sequence information for
 * this folder from the profile/context list.
 */

static void
rma (char *folder)
{
    register int alen, j, plen;
    register char *cp;
    register struct node *np, *pp;

    /* sanity check - check that context has been read */
    if (defpath == NULL)
	adios (NULL, "oops, context hasn't been read yet");

    alen = strlen ("atr-");
    plen = strlen (cp = m_mailpath (folder)) + 1;

    /*
     * Search context list for keys that look like
     * "atr-something-folderpath", and remove them.
     */
    for (np = m_defs, pp = NULL; np; np = np->n_next) {
	if (ssequal ("atr-", np->n_name)
		&& (j = strlen (np->n_name) - plen) > alen
		&& *(np->n_name + j) == '-'
		&& strcmp (cp, np->n_name + j + 1) == 0) {
	    if (!np->n_context)
		admonish (NULL, "bug: context_del(key=\"%s\")", np->n_name);
	    if (pp) {
		pp->n_next = np->n_next;
		np = pp;
	    } else {
		m_defs = np->n_next;
	    }
	    ctxflags |= CTXMOD;
	} else {
	    pp = np;
	}
    }
}
