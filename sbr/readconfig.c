
/*
 * readconfig.c -- base routine to read nmh configuration files
 *              -- such as nmh profile, context file, or mhn.defaults.
 *
 * $Id$
 */

#include <h/mh.h>

struct procstr {
    char *procname;
    char **procnaddr;
};

static struct procstr procs[] = {
    { "context",       &context },
    { "mh-sequences",  &mh_seq },
    { "buildmimeproc", &buildmimeproc },
    { "faceproc",      &faceproc },
    { "fileproc",      &fileproc },
    { "incproc",       &incproc },
    { "installproc",   &installproc },
    { "lproc",         &lproc },
    { "mailproc",      &mailproc },
    { "mhlproc",       &mhlproc },
    { "moreproc",      &moreproc },
    { "mshproc",       &mshproc },
    { "packproc",      &packproc },
    { "postproc",      &postproc },
    { "rmfproc",       &rmfproc },
    { "rmmproc",       &rmmproc },
    { "sendproc",      &sendproc },
    { "showmimeproc",  &showmimeproc },
    { "showproc",      &showproc },
    { "vmhproc",       &vmhproc },
    { "whatnowproc",   &whatnowproc },
    { "whomproc",      &whomproc },
    { NULL,            NULL }
};

static struct node **opp = NULL;


void
readconfig (struct node **npp, FILE *ib, char *file, int ctx)
{
    register int state;
    register char *cp;
    char name[NAMESZ], field[BUFSIZ];
    register struct node *np;
    register struct procstr *ps;

    if (npp == NULL && (npp = opp) == NULL) {
	admonish (NULL, "bug: readconfig called but pump not primed");
	return;
    }

    for (state = FLD;;) {
	switch (state = m_getfld (state, name, field, sizeof(field), ib)) {
	    case FLD:
	    case FLDPLUS:
	    case FLDEOF:
		if (!(np = (struct node *) malloc (sizeof(*np))))
		    adios (NULL, "unable to allocate profile storage");
		*npp = np;
		*(npp = &np->n_next) = NULL;
		np->n_name = getcpy (name);
		if (state == FLDPLUS) {
		    cp = getcpy (field);
		    while (state == FLDPLUS) {
			state = m_getfld (state, name, field, sizeof(field), ib);
			cp = add (field, cp);
		    }
		    np->n_field = trimcpy (cp);
		    free (cp);
		} else {
		    np->n_field = trimcpy (field);
		}
		np->n_context = ctx;

		/*
		 * Now scan the list of `procs' and link in the
		 * field value to the global variable.
		 */
		for (ps = procs; ps->procname; ps++)
		    if (strcmp (np->n_name, ps->procname) == 0) {
			*ps->procnaddr = np->n_field;
			break;
		    }
		if (state == FLDEOF)
		    break;
		continue;

	    case BODY:
	    case BODYEOF:
		adios (NULL, "no blank lines are permitted in %s", file);

	    case FILEEOF:
		break;

	    default:
		adios (NULL, "%s is poorly formatted", file);
	}
	break;
    }

    opp = npp;
}
