
/*
 * mhparam.c -- print mh_profile values
 *
 * Originally contributed by
 * Jeffrey C Honig <Jeffrey_C_Honig@cornell.edu>
 *
 * $Id$
 */

#include <h/mh.h>

extern char *mhlibdir;
extern char *mhetcdir;

char *sbackup = BACKUP_PREFIX;
char *slink = LINK;

static struct swit switches[] = {
#define	COMPSW	  0
    { "components", 0 },
#define	NCOMPSW	  1
    { "nocomponents", 0 },
#define	ALLSW	  2
    { "all", 0 },
#define VERSIONSW 3
    { "version", 0 },
#define	HELPSW	  4
    { "help", 4 },
#define DEBUGSW   5
    { "debug", -5 },
    { NULL, 0 }
};

struct proc {
    char *p_name;
    char **p_field;
};

static struct proc procs [] = {
     { "context",       &context },
     { "mh-sequences",  &mh_seq },
     { "buildmimeproc", &buildmimeproc },
     { "faceproc",      &faceproc },
     { "fileproc",      &fileproc },
     { "foldprot",      &foldprot },
     { "incproc",       &incproc },
     { "installproc",   &installproc  },
     { "lproc",         &lproc },
     { "mailproc",      &mailproc },
     { "mhlproc",       &mhlproc },
     { "moreproc",      &moreproc },
     { "msgprot",       &msgprot },
     { "mshproc",       &mshproc },
     { "packproc",      &packproc },
     { "postproc",      &postproc },
     { "rmfproc",       &rmfproc },
     { "rmmproc",       &rmmproc },
     { "sendproc",      &sendproc },
     { "showmimeproc",  &showmimeproc },
     { "showproc",      &showproc },
     { "version",       &version_num },
     { "vmhproc",       &vmhproc },
     { "whatnowproc",   &whatnowproc },
     { "whomproc",      &whomproc },
     { "etcdir",        &mhetcdir },
     { "libdir",        &mhlibdir },
     { "sbackup",       &sbackup },
     { "link",          &slink },
     { NULL,            NULL },
};


/*
 * static prototypes
 */
static char *p_find(char *);


int
main(int argc, char **argv)
{
    int i, compp = 0, missed = 0;
    int all = 0, debug = 0;
    int components = -1;
    char *cp, buf[BUFSIZ], **argp;
    char **arguments, *comps[MAXARGS];

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
		    snprintf (buf, sizeof(buf), "%s [profile-components] [switches]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case COMPSW:
		    components = 1;
		    break;
		case NCOMPSW:
		    components = 0;
		    break;

		case ALLSW:
		    all = 1;
		    break;

		case DEBUGSW:
		    debug = 1;
		    break;
	    }
	} else {
	    comps[compp++] = cp;
	}
    }

    if (all) {
        struct node *np;

	if (compp)
	    advise(NULL, "profile-components ignored with -all");

	if (components >= 0)
	    advise(NULL, "-%scomponents ignored with -all",
		   components ? "" : "no");

	/* print all entries in context/profile list */
	for (np = m_defs; np; np = np->n_next)
	    printf("%s: %s\n", np->n_name, np->n_field);

    } if (debug) {
	struct proc *ps;

	/*
	 * Print the current value of everything in
	 * procs array.  This will show their current
	 * value (as determined after context is read).
         */
	for (ps = procs; ps->p_name; ps++)
	    printf ("%s: %s\n", ps->p_name, *ps->p_field ? *ps->p_field : "");

    } else {
        if (components < 0)
	    components = compp > 1;

	for (i = 0; i < compp; i++)  {
	    register char *value;

	    value = context_find (comps[i]);
	    if (!value)
		value = p_find (comps[i]);
	    if (value) {
	        if (components)
		    printf("%s: ", comps[i]);

		printf("%s\n", value);
	    } else
	        missed++;
	}
    }
    
    return done (missed);
}


static char *
p_find(char *str)
{
    struct proc *ps;

    for (ps = procs; ps->p_name; ps++)
	if (!strcasecmp (ps->p_name, str))
	    return (*ps->p_field);

    return NULL;
}
