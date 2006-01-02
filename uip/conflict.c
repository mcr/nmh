
/*
 * conflict.c -- check for conflicts in mail system
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/aliasbr.h>
#include <h/mts.h>
#include <h/utils.h>
#include <grp.h>
#include <pwd.h>

/*
 * maximum number of directories that can
 * be specified using -search switch.
 */
#define	NDIRS	100

/*
 * Add space for group names, 100 at a time
 */
#define	NGRPS	100

static struct swit switches[] = {
#define	MAILSW         0
    { "mail name", 0 },
#define	SERCHSW        1
    { "search directory", 0 },
#define VERSIONSW      2
    { "version", 0 },
#define	HELPSW         3
    { "help", 0 },
    { NULL, 0 }
};

static char *mail = NULL;
static char *dirs[NDIRS];
static FILE *out = NULL;

extern struct aka  *akahead;
extern struct home *homehead;

/*
 * prototypes
 */
void alias_files (int, char **);
void pwd_names (void);
void grp_names (void);
void grp_members (void);
void grp_ids (void);
void maildrops (void);
void mdrop(char *);
int check (char *);
void setup (void);


int
main (int argc, char **argv)
{
    int	akp = 0, dp = 0;
    char *cp, **argp, **arguments;
    char buf[BUFSIZ], *akv[50];

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* foil search of user profile/context */
    if (context_foil (NULL) == -1)
	done (1);

    mts_init (invo_name);
    arguments = getarguments (invo_name, argc, argv, 0);
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
		    snprintf (buf, sizeof(buf), "%s [switches] [aliasfiles ...]",
			invo_name);
		    print_help (buf, switches, 0);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case MAILSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if (mail)
			adios (NULL, "mail to one address only");
		    else
			mail = cp;
		    continue;

		case SERCHSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if (dp >= NDIRS)
			adios (NULL, "more than %d directories", NDIRS);
		    dirs[dp++] = cp;
		    continue;
	    }
	}
	akv[akp++] = cp;
    }

    if (akp == 0)
	akv[akp++] = AliasFile;
    if (!homehead)
	init_pw ();
    if (!mail)
	out = stdout;
    dirs[dp] = NULL;

    alias_files (akp, akv);
    pwd_names ();
    grp_names ();
    grp_members ();
    grp_ids ();
#ifdef	UCI
    ldr_names ();
    ldr_ship ();
#endif	/* UCI */
    maildrops ();

    return done (0);
}


void
alias_files (int akp, char **akv)
{
    register int i, err;

    for (i = 0; i < akp; i++)
	if ((err = alias (akv[i])) != AK_OK) {
	    setup ();
	    fprintf (out, "aliasing error in %s - %s\n", akv[i], akerror (err));
	}
	else
	    if (out && !mail)
		fprintf (out, "alias file %s is ok\n", akv[i]);
}


void
pwd_names (void)
{
    int hit = 0;
    register struct home *hm, *lm;

    for (hm = homehead; hm; hm = hm->h_next)
	for (lm = hm->h_next; lm; lm = lm->h_next)
	    if (strcmp (hm->h_name, lm->h_name) == 0) {
		setup ();
		fprintf (out, "duplicate user %s(uid=%d)\n",
			lm->h_name, (int) lm->h_uid);
		hit++;
	    }

    if (!hit && out && !mail)
	fprintf (out, "no duplicate users\n");
}


void
grp_names (void)
{
    int numgroups, maxgroups;
    int i, hit = 0;
    char **grps;
    struct group *gr;

    /* allocate space NGRPS at a time */
    numgroups = 0;
    maxgroups = NGRPS;
    grps = (char **) mh_xmalloc((size_t) (maxgroups * sizeof(*grps)));

    setgrent ();
    while ((gr = getgrent ())) {
	for (i = 0; i < numgroups; i++)
	    if (!strcmp (grps[i], gr->gr_name)) {
		setup ();
		fprintf (out, "duplicate group %s(gid=%d)\n",
			gr->gr_name, (int) gr->gr_gid);
		hit++;
		break;
	    }
	if (i >= numgroups) {
	    if (numgroups >= maxgroups) {
		maxgroups += NGRPS;
		grps = (char **) mh_xrealloc(grps,
		    (size_t) (maxgroups * sizeof(*grps)));
	    }
	    grps[numgroups++] = getcpy (gr->gr_name);
	}
    }
    endgrent ();

    for (i = 0; i < numgroups; i++)
	free (grps[i]);
    free (grps);

    if (!hit && out && !mail)
	fprintf (out, "no duplicate groups\n");
}


void
grp_members (void)
{
    register int hit = 0;
    register char **cp, **dp;
    register struct group *gr;
    register struct home  *hm;

    setgrent ();
    while ((gr = getgrent ())) {
	for (cp = gr->gr_mem; *cp; cp++) {
	    for (hm = homehead; hm; hm = hm->h_next)
		if (!strcmp (*cp, hm->h_name))
		    break;
	    if (hm == NULL) {
		setup ();
		fprintf (out, "group %s(gid=%d) has unknown member %s\n",
			gr->gr_name, (int) gr->gr_gid, *cp);
		hit++;
	    } else {
		hm->h_ngrps++;
	    }

	    for (dp = cp + 1; *dp; dp++)
		if (strcmp (*cp, *dp) == 0) {
		    setup ();
		    fprintf (out, "group %s(gid=%d) has duplicate member %s\n",
			    gr->gr_name, (int) gr->gr_gid, *cp);
		    hit++;
		}
	}
    }
    endgrent ();

    for (hm = homehead; hm; hm = hm->h_next)
	if (hm->h_ngrps > NGROUPS_MAX) {
	    setup ();
	    fprintf (out, "user %s is a member of %d groups (max %d)\n",
		    hm->h_name, hm->h_ngrps, NGROUPS_MAX);
	    hit++;
	}

    if (!hit && out && !mail)
	fprintf (out, "all group members accounted for\n");
}


void
grp_ids (void)
{		/* -DRAND not implemented at most places */
    register int hit = 0;
    register struct home *hm;

    for (hm = homehead; hm; hm = hm->h_next)
	if (getgrgid (hm->h_gid) == NULL) {
	    setup ();
	    fprintf (out, "user %s(uid=%d) has unknown group-id %d\n",
		    hm->h_name, (int) hm->h_uid, (int) hm->h_gid);
	    hit++;
	}

    if (!hit && out && !mail)
	fprintf (out, "all group-id users accounted for\n");
}


void
maildrops (void) 
{
    register int i;

    if (mmdfldir && *mmdfldir)
	mdrop (mmdfldir);
    if (uucpldir && *uucpldir)
	mdrop (uucpldir);
    for (i = 0; dirs[i]; i++)
	mdrop (dirs[i]);
}


void
mdrop(char *drop)
{
    register int hit = 0;
    register struct dirent *dp;
    register DIR *dd = opendir (drop);

    if (!dd) {
	setup ();
	fprintf (out, "unable to open maildrop area %s\n", drop);
	return;
    }

    while ((dp = readdir (dd)))
	if (dp->d_name[0] != '.' && !check (dp->d_name)) {
	    setup ();
	    fprintf (out,
		    "there is a maildrop for the unknown user %s in %s\n",
		    dp->d_name, drop);
	    hit++;
	}

    closedir (dd);
    if (!hit && out && !mail)
	fprintf (out, "all maildrops accounted for in %s\n", drop);
}


int
check (char *s)
{
    register struct home *hm;

    for (hm = homehead; hm; hm = hm->h_next)
	if (!strcmp (s, hm->h_name))
	    return 1;
    return 0;
}

void
setup (void)
{
    int fd, pd[2];

    if (out)
	return;

    if (mail) {
	if (pipe (pd) == NOTOK)
	    adios ("pipe", "unable to");

	switch (fork ()) {
	    case NOTOK: 
		adios ("fork", "unable to");

	    case OK: 
		close (pd[1]);
		if (pd[0] != 0) {
		    dup2 (pd[0], 0);
		    close (pd[0]);
		}
		if ((fd = open ("/dev/null", O_WRONLY)) != NOTOK)
		    if (fd != 1) {
			dup2 (fd, 1);
			close (fd);
		    }
		execlp (mailproc, r1bindex (mailproc, '/'),
			mail, "-subject", invo_name, NULL);
		adios (mailproc, "unable to exec ");

	    default: 
		close (pd[0]);
		out = fdopen (pd[1], "w");
		fprintf (out, "%s: the following is suspicious\n\n",
			invo_name);
	}
    }
}

#ifdef UCI
/*
 * UCI specific stuff for conflict
 */

/* taken from <grpldr.h> */

#define	GLDRS	"/admin/etc/GroupLeaders"

struct grpldr {
    char *gl_name;
    char **gl_ldr;
};

int setglent (), endglent ();
struct grpldr *getglent (), *getglnam ();


/* taken from the getglent() routines */

#define	MAXGLS	100

static FILE *glp = NULL;
static char line[BUFSIZ+1];
static struct grpldr grpldr;
static char *gl_ldr[MAXGLS + 1];


setglent() {
    if (glp == NULL)
	glp = fopen (GLDRS, "r");
    else
	rewind (glp);

    return (glp != NULL);
}


endglent() {
    if (glp != NULL) {
	fclose (glp);
	glp = NULL;
    }

    return 1;
}

struct grpldr  *getglent () {
    register char  *cp,
                  **q;

    if (glp == NULL && !setglent ())
	return NULL;
    if ((cp = fgets (line, BUFSIZ, glp)) == NULL)
	return NULL;

    grpldr.gl_name = cp;
    grpldr.gl_ldr = q = gl_ldr;

    while (*cp) {
	while (*cp && !isspace (*cp))
	    cp++;
	while (*cp && isspace (*cp))
	    *cp++ = '\0';
	if (*cp == '\0')
	    break;
	if (q < gl_ldr + MAXGLS)
	    *q++ = cp;
	else
	    break;
    }
    *q = NULL;

    return (&grpldr);
}

struct grpldr  *getglnam (name)
char   *name;
{
    register struct grpldr  *gl = NULL;

    setglent ();
    while (gl = getglent ())
	if (strcmp (name, gl->gl_name) == 0)
	    break;
    endglent ();

    return gl;
}

ldr_names () {
    register int     gp,
		     hit = 0;
    char   *gldrs[NGRPS];
    register struct grpldr  *gl;

    gldrs[0] = NULL;
    setglent ();
    while (gl = getglent ()) {
	if (getgrnam (gl->gl_name) == NULL) {
	    setup ();
	    fprintf (out, "unknown group %s in group leaders file\n",
		    gl->gl_name);
	    hit++;
	}
	for (gp = 0; gldrs[gp]; gp++)
	    if (strcmp (gldrs[gp], gl->gl_name) == 0) {
		setup ();
		fprintf (out, "duplicate group %s in group leaders file\n",
			gl->gl_name);
		hit++;
		break;
	    }
	if (gldrs[gp] == NULL)
	    if (gp < NGRPS) {
		gldrs[gp++] = getcpy (gl->gl_name);
		gldrs[gp] = NULL;
	    }
	    else {
		setup ();
		fprintf (out, "more than %d groups in group leaders file%s\n",
			" (time to recompile)", NGRPS - 1);
		hit++;
	    }
    }
    endglent ();

    for (gp = 0; gldrs[gp]; gp++)
	free (gldrs[gp]);

    if (!hit && out && !mail)
	fprintf (out, "all groups in group leaders file accounted for\n");
}


ldr_ship () {
    register int     hit = 0;
    register char  **cp,
		   **dp;
    register struct grpldr  *gl;

    setglent ();
    while (gl = getglent ())
	for (cp = gl->gl_ldr; *cp; cp++) {
	    if (!check (*cp)) {
		setup ();
		fprintf (out, "group %s has unknown leader %s\n",
			gl->gl_name, *cp);
		hit++;
	    }

	    for (dp = cp + 1; *dp; dp++)
		if (strcmp (*cp, *dp) == 0) {
		    setup ();
		    fprintf (out, "group %s had duplicate leader %s\n",
			    gl->gl_name, *cp);
		    hit++;
		}
	}
    endglent ();

    if (!hit && out && !mail)
	fprintf (out, "all group leaders accounted for\n");
}
#endif	/* UCI */
