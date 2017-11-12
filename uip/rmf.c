/* rmf.c -- remove a folder
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/concat.h"
#include "sbr/smatch.h"
#include "sbr/remdir.h"
#include "sbr/ssequal.h"
#include "sbr/m_atoi.h"
#include "sbr/getfolder.h"
#include "sbr/ext_hook.h"
#include "sbr/context_save.h"
#include "sbr/context_replace.h"
#include "sbr/context_del.h"
#include "sbr/context_find.h"
#include "sbr/ambigsw.h"
#include "sbr/path.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/error.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/m_maildir.h"
#include "sbr/m_mktemp.h"

#define RMF_SWITCHES \
    X("interactive", 0, INTRSW) \
    X("nointeractive", 0, NINTRSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(RMF);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(RMF, switches);
#undef X

/*
 * static prototypes
 */
static int rmf(char *);
static void rma (char *);


int
main (int argc, char **argv)
{
    bool defolder = false;
    int interactive = -1;
    char *cp, *folder = NULL, newfolder[BUFSIZ];
    char buf[BUFSIZ], **argp, **arguments;
    char *fp;

    if (nmh_init(argv[0], true, true)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    die("-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [+folder] [switches]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

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
		die("only one folder at a time!");
            folder = pluspath (cp);
	} else {
	    die("usage: %s [+folder] [switches]", invo_name);
	}
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (!folder) {
	folder = getfolder (1);
	defolder = true;
    }
    fp = m_mailpath(folder);
    if (!strcmp(fp, pwd()))
	die("sorry, you can't remove the current working directory");
    free(fp);

    if (interactive == -1)
	interactive = defolder;

    if (strchr (folder, '/') && (*folder != '/') && (*folder != '.')) {
	for (cp = stpcpy(newfolder, folder); cp > newfolder && *cp != '/'; cp--)
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
	if (!read_yes_or_no_if_tty (cp))
	    done (0);
	free (cp);
    }

    if (rmf (folder) == OK) {
	char *cfolder = context_find(pfolder);
	if (cfolder && strcmp (cfolder, newfolder)) {
	    printf ("[+%s now current]\n", newfolder);
	    context_replace (pfolder, newfolder);	/* update current folder */
	}
    }
    context_save ();	/* save the context file */
    done (0);
    return 1;
}

static int
rmf (char *folder)
{
    int i;
    bool others;
    char *fp;
    char *maildir;
    char cur[BUFSIZ];
    struct dirent *dp;
    DIR *dd;

    switch (i = chdir (maildir = m_maildir (folder))) {
	case OK: 
	    if (access (".", W_OK) != NOTOK && access ("..", W_OK) != NOTOK)
		break;
	    /* FALLTHRU */

	case NOTOK: 
            fp = m_mailpath(folder);
	    snprintf (cur, sizeof(cur), "atr-%s-%s", current, fp);
            free(fp);
	    if (!context_del (cur)) {
		printf ("[+%s de-referenced]\n", folder);
		return OK;
	    }
	    inform("you have no profile entry for the %s folder +%s",
		    i == NOTOK ? "unreadable" : "read-only", folder);
	    return NOTOK;
    }

    if ((dd = opendir (".")) == NULL)
	die("unable to read folder +%s", folder);
    others = false;

    /*
     *	Run the external delete hook program.
     */

    (void)ext_hook("del-hook", maildir, NULL);

    while ((dp = readdir (dd))) {
	switch (dp->d_name[0]) {
	    case '.': 
		if (strcmp (dp->d_name, ".") == 0
			|| strcmp (dp->d_name, "..") == 0)
		    continue;
		break;

	    case ',': 
		break;

	    default: 
		if (m_atoi (dp->d_name))
		    break;
		if (strcmp (dp->d_name, LINK) == 0
			|| has_prefix(dp->d_name, BACKUP_PREFIX))
		    break;

		inform("file \"%s/%s\" not deleted, continuing...",
			folder, dp->d_name);
		others = true;
		continue;
	}
	if (m_unlink (dp->d_name) == NOTOK) {
	    admonish (dp->d_name, "unable to unlink %s:", folder);
	    others = true;
	}
    }

    closedir (dd);

    /*
     * Remove any relevant private sequences
     * or attributes from context file.
     */
    rma (folder);

    if (chdir ("..") < 0) {
	advise ("..", "chdir");
    }
    if (!others && remdir (maildir))
	return OK;

    inform("folder +%s not removed", folder);
    return NOTOK;
}


/*
 * Remove all the (private) sequence information for
 * this folder from the profile/context list.
 */

static void
rma (char *folder)
{
    int alen, j, plen;
    char *cp;
    struct node *np, *pp;

    alen = LEN("atr-");
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
		inform("bug: context_del(key=\"%s\"), continuing...",
		    np->n_name);
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

    free(cp);
}
