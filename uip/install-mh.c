
/*
 * install-mh.c -- initialize the nmh environment of a new user
 *
 * $Id$
 */

#include <h/mh.h>
#include <pwd.h>

static struct swit switches[] = {
#define AUTOSW     0
    { "auto", 0 },
#define VERSIONSW  1
    { "version", 0 },
#define HELPSW     2
    { "help", 0 },
    { NULL, 0 }
};

static char *message[] = {
    "Prior to using nmh, it is necessary to have a file in your login",
    "directory (%s) named %s which contains information",
    "to direct certain nmh operations.  The only item which is required",
    "is the path to use for all nmh folder operations.  The suggested nmh",
    "path for you is %s/Mail...",
    NULL
};

/*
 * static prototypes
 */
static char *geta(void);


int
main (int argc, char **argv)
{
    int i, autof = 0;
    char *cp, *path, buf[BUFSIZ];
    char *dp, **arguments, **argp;
    struct node *np;
    struct passwd *pw;
    struct stat st;
    FILE *in, *out;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');
    arguments = getarguments (invo_name, argc, argv, 0);
    argp = arguments;

    while ((dp = *argp++)) {
	if (*dp == '-') {
	    switch (smatch (++dp, switches)) {
		case AMBIGSW:
		    ambigsw (dp, switches);
		    done (1);
		case UNKWNSW:
		    adios (NULL, "-%s unknown\n", dp);

		case HELPSW:
		    snprintf (buf, sizeof(buf), "%s [switches]", invo_name);
		    print_help (buf, switches, 0);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case AUTOSW:
		    autof++;
		    continue;
	    }
	} else {
	    adios (NULL, "%s is invalid argument", dp);
	}
    }

    /* straight from context_read ... */
    if (mypath == NULL) {
	if ((mypath = getenv ("HOME"))) {
	    mypath = getcpy (mypath);
	} else {
	    if ((pw = getpwuid (getuid ())) == NULL
		    || pw->pw_dir == NULL
		    || *pw->pw_dir == 0)
		adios (NULL, "no HOME envariable");
	    else
		mypath = getcpy (pw->pw_dir);
	}
	if ((cp = mypath + strlen (mypath) - 1) > mypath && *cp == '/')
	    *cp = 0;
    }
    defpath = concat (mypath, "/", mh_profile, NULL);

    if (stat (defpath, &st) != NOTOK) {
	if (autof)
	    adios (NULL, "invocation error");
	else
	    adios (NULL,
		    "You already have an nmh profile, use an editor to modify it");
    }

    if (!autof && gans ("Do you want help? ", anoyes)) {
	putchar ('\n');
	for (i = 0; message[i]; i++) {
	    printf (message[i], mypath, mh_profile);
	    putchar ('\n');
	}
	putchar ('\n');
    }

    cp = concat (mypath, "/", "Mail", NULL);
    if (stat (cp, &st) != NOTOK) {
	if (S_ISDIR(st.st_mode)) {
	    cp = concat ("You already have the standard nmh directory \"",
		    cp, "\".\nDo you want to use it for nmh? ", NULL);
	    if (gans (cp, anoyes))
		path = "Mail";
	    else
		goto query;
	} else {
	    goto query;
	}
    } else {
	if (autof)
	    printf ("I'm going to create the standard nmh path for you.\n");
	else
	    cp = concat ("Do you want the standard nmh path \"",
		    mypath, "/", "Mail\"? ", NULL);
	if (autof || gans (cp, anoyes))
	    path = "Mail";
	else {
query:
	    if (gans ("Do you want a path below your login directory? ",
			anoyes)) {
		printf ("What is the path?  %s/", mypath);
		path = geta ();
	    } else {
		printf ("What is the whole path?  /");
		path = concat ("/", geta (), NULL);
	    }
	}
    }

    chdir (mypath);
    if (chdir (path) == NOTOK) {
	cp = concat ("\"", path, "\" doesn't exist; Create it? ", NULL);
	if (autof || gans (cp, anoyes))
	    if (makedir (path) == 0)
		adios (NULL, "unable to create %s", path);
    } else {
	printf ("[Using existing directory]\n");
    }

    /*
     * Add some initial elements to the profile/context list
     */
    if (!(m_defs = (struct node *) malloc (sizeof *np)))
	adios (NULL, "unable to allocate profile storage");
    np = m_defs;
    np->n_name = getcpy ("Path");
    np->n_field = getcpy (path);
    np->n_context = 0;
    np->n_next = NULL;

    /*
     * If there is a default profile file in the
     * nmh `etc' directory, then read it also.
     */
    if ((in = fopen (mh_defaults, "r"))) {
	readconfig (&np->n_next, in, mh_defaults, 0);
	fclose (in);
    }

    ctxpath = getcpy (m_maildir (context = "context"));

    /* Initialize current folder to default */
    context_replace (pfolder, defaultfolder);
    context_save ();

    /*
     * Now write out the initial .mh_profile
     */
    if ((out = fopen (defpath, "w")) == NULL)
	adios (defpath, "unable to write");
    for (np = m_defs; np; np = np->n_next) {
	if (!np->n_context)
	    fprintf (out, "%s: %s\n", np->n_name, np->n_field);
    }
    fclose (out);
    return done (0);
}


static char *
geta (void)
{
    char *cp;
    static char line[BUFSIZ];

    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) == NULL)
	done (1);
    if ((cp = strchr(line, '\n')))
	*cp = 0;
    return line;
}
