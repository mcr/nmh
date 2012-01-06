
/*
 * aliasbr.c -- new aliasing mechanism
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/aliasbr.h>
#include <h/utils.h>
#include <grp.h>
#include <pwd.h>

static int akvis;
static char *akerrst;

struct aka *akahead = NULL;
struct aka *akatail = NULL;

struct home *homehead = NULL;
struct home *hometail = NULL;

/*
 * prototypes
 */
int alias (char *); 
int akvisible (void);
void init_pw (void);
char *akresult (struct aka *);
char *akvalue (char *);
char *akerror (int);

static  char *akval (struct aka *, char *);
static int aleq (char *, char *);
static char *scanp (unsigned char *);
static char *getp (char *);
static char *seekp (char *, char *, char **);
static int addfile (struct aka *, char *);
static int addgroup (struct aka *, char *);
static int addmember (struct aka *, char *);
static int addall (struct aka *);
static char *getalias (char *);
static void add_aka (struct aka *, char *);
static struct aka *akalloc (char *);
static struct home *hmalloc (struct passwd *);
struct home *seek_home (char *);


/* Do mh alias substitution on 's' and return the results. */
char *
akvalue (char *s)
{
    register char *v;

    if (akahead == NULL)
	alias (AliasFile);

    akvis = -1;
    v = akval (akahead, s);
    if (akvis == -1)
	akvis = 0;
    return v;
}


int
akvisible (void)
{
    return akvis;
}


char *
akresult (struct aka *ak)
{
    register char *cp = NULL, *dp, *pp;
    register struct adr *ad;

    for (ad = ak->ak_addr; ad; ad = ad->ad_next) {
	pp = ad->ad_local ? akval (ak->ak_next, ad->ad_text)
	    : getcpy (ad->ad_text);

	if (cp) {
	    dp = cp;
	    cp = concat (cp, ",", pp, NULL);
	    free (dp);
	    free (pp);
	}
	else
	    cp = pp;
    }

    if (akvis == -1)
	akvis = ak->ak_visible;
    return cp;
}


static	char *
akval (struct aka *ak, char *s)
{
    if (!s)
	return s;			/* XXX */

    for (; ak; ak = ak->ak_next)
	if (aleq (s, ak->ak_name))
	    return akresult (ak);

    return getcpy (s);
}


static int
aleq (char *string, char *aliasent)
{
    register char c;

    while ((c = *string++))
	if (*aliasent == '*')
	    return 1;
	else
	    if ((c | 040) != (*aliasent | 040))
		return 0;
	    else
		aliasent++;

    return (*aliasent == 0 || *aliasent == '*');
}


int
alias (char *file)
{
    int i;
    register char *bp, *cp, *pp;
    char lc, *ap;
    register struct aka *ak = NULL;
    register FILE *fp;

    if (*file != '/'
	    && (strncmp (file, "./", 2) && strncmp (file, "../", 3)))
	file = etcpath (file);
    if ((fp = fopen (file, "r")) == NULL) {
	akerrst = file;
	return AK_NOFILE;
    }

    while (vfgets (fp, &ap) == OK) {
	bp = ap;
	switch (*(pp = scanp (bp))) {
	    case '<': 		/* recurse a level */
		if (!*(cp = getp (pp + 1))) {
		    akerrst = "'<' without alias-file";
		    fclose (fp);
		    return AK_ERROR;
		}
		if ((i = alias (cp)) != AK_OK) {
		    fclose (fp);
		    return i;
		}

	    case ':': 		/* comment */
	    case ';': 
	    case '#':
	    case 0: 
		continue;
	}

	akerrst = bp;
	if (!*(cp = seekp (pp, &lc, &ap))) {
	    fclose (fp);
	    return AK_ERROR;
	}
	if (!(ak = akalloc (cp))) {
	    fclose (fp);
	    return AK_LIMIT;
	}
	switch (lc) {
	    case ':': 
		ak->ak_visible = 0;
		break;

	    case ';': 
		ak->ak_visible = 1;
		break;

	    default: 
		fclose (fp);
		return AK_ERROR;
	}

	switch (*(pp = scanp (ap))) {
	    case 0: 		/* EOL */
		fclose (fp);
		return AK_ERROR;

	    case '<': 		/* read values from file */
		if (!*(cp = getp (pp + 1))) {
		    fclose (fp);
		    return AK_ERROR;
		}
		if (!addfile (ak, cp)) {
		    fclose (fp);
		    return AK_NOFILE;
		}
		break;

	    case '=': 		/* UNIX group */
		if (!*(cp = getp (pp + 1))) {
		    fclose (fp);
		    return AK_ERROR;
		}
		if (!addgroup (ak, cp)) {
		    fclose (fp);
		    return AK_NOGROUP;
		}
		break;

	    case '+': 		/* UNIX group members */
		if (!*(cp = getp (pp + 1))) {
		    fclose (fp);
		    return AK_ERROR;
		}
		if (!addmember (ak, cp)) {
		    fclose (fp);
		    return AK_NOGROUP;
		}
		break;

	    case '*': 		/* Everyone */
		addall (ak);
		break;

	    default: 		/* list */
		while ((cp = getalias (pp)))
		    add_aka (ak, cp);
		break;
	}
    }

    fclose (fp);
    return AK_OK;
}


char *
akerror (int i)
{
    static char buffer[BUFSIZ];

    switch (i) {
	case AK_NOFILE: 
	    snprintf (buffer, sizeof(buffer), "unable to read '%s'", akerrst);
	    break;

	case AK_ERROR: 
	    snprintf (buffer, sizeof(buffer), "error in line '%s'", akerrst);
	    break;

	case AK_LIMIT: 
	    snprintf (buffer, sizeof(buffer), "out of memory while on '%s'", akerrst);
	    break;

	case AK_NOGROUP: 
	    snprintf (buffer, sizeof(buffer), "no such group as '%s'", akerrst);
	    break;

	default: 
	    snprintf (buffer, sizeof(buffer), "unknown error (%d)", i);
	    break;
    }

    return buffer;
}


static char *
scanp (unsigned char *p)
{
    while (isspace (*p))
	p++;
    return p;
}


static char *
getp (char *p)
{
    register unsigned char  *cp = scanp (p);

    p = cp;
    while (!isspace (*cp) && *cp)
	cp++;
    *cp = 0;

    return p;
}


static char *
seekp (char *p, char *c, char **a)
{
    register unsigned char *cp;

    p = cp = scanp (p);
    while (!isspace (*cp) && *cp && *cp != ':' && *cp != ';')
	cp++;
    *c = *cp;
    *cp++ = 0;
    *a = cp;

    return p;
}


static int
addfile (struct aka *ak, char *file)
{
    register char *cp;
    char buffer[BUFSIZ];
    register FILE *fp;

    if (!(fp = fopen (etcpath (file), "r"))) {
	akerrst = file;
	return 0;
    }

    while (fgets (buffer, sizeof buffer, fp))
	while ((cp = getalias (buffer)))
	    add_aka (ak, cp);

    fclose (fp);
    return 1;
}


static int
addgroup (struct aka *ak, char *grp)
{
    register char *gp;
    register struct group *gr = getgrnam (grp);
    register struct home *hm = NULL;

    if (!gr)
	gr = getgrgid (atoi (grp));
    if (!gr) {
	akerrst = grp;
	return 0;
    }

    while ((gp = *gr->gr_mem++))
    {
	struct passwd *pw;
	for (hm = homehead; hm; hm = hm->h_next)
	    if (!strcmp (hm->h_name, gp)) {
		add_aka (ak, hm->h_name);
		break;
	    }
        if ((pw = getpwnam(gp)))
	{
		hmalloc(pw);
		add_aka (ak, gp);
	}
    }

    return 1;
}


static int
addmember (struct aka *ak, char *grp)
{
    gid_t gid;
    register struct group *gr = getgrnam (grp);
    register struct home *hm = NULL;

    if (gr)
	gid = gr->gr_gid;
    else {
	gid = atoi (grp);
	gr = getgrgid (gid);
    }
    if (!gr) {
	akerrst = grp;
	return 0;
    }

    init_pw ();

    for (hm = homehead; hm; hm = hm->h_next)
	if (hm->h_gid == gid)
	    add_aka (ak, hm->h_name);

    return 1;
}


static int
addall (struct aka *ak)
{
    int noshell = NoShell == NULL || *NoShell == 0;
    register struct home *hm;

    init_pw ();

    if (Everyone < 0)
	Everyone = EVERYONE;

    for (hm = homehead; hm; hm = hm->h_next)
	if (hm->h_uid > Everyone
		&& (noshell || strcmp (hm->h_shell, NoShell)))
	    add_aka (ak, hm->h_name);

    return homehead != NULL;
}


static char *
getalias (char *addrs)
{
    register unsigned char *pp, *qp;
    static char *cp = NULL;

    if (cp == NULL)
	cp = addrs;
    else
	if (*cp == 0)
	    return (cp = NULL);

    for (pp = cp; isspace (*pp); pp++)
	continue;
    if (*pp == 0)
	return (cp = NULL);
    for (qp = pp; *qp != 0 && *qp != ','; qp++)
	continue;
    if (*qp == ',')
	*qp++ = 0;
    for (cp = qp, qp--; qp > pp; qp--)
	if (*qp != 0) {
	    if (isspace (*qp))
		*qp = 0;
	    else
		break;
	}

    return pp;
}


static void
add_aka (struct aka *ak, char *pp)
{
    register struct adr *ad, *ld;

    for (ad = ak->ak_addr, ld = NULL; ad; ld = ad, ad = ad->ad_next)
	if (!strcmp (pp, ad->ad_text))
	    return;

    ad = (struct adr *) mh_xmalloc (sizeof(*ad));
    ad->ad_text = getcpy (pp);
    ad->ad_local = strchr(pp, '@') == NULL && strchr(pp, '!') == NULL;
    ad->ad_next = NULL;
    if (ak->ak_addr)
	ld->ad_next = ad;
    else
	ak->ak_addr = ad;
}


void
init_pw (void)
{
    register struct passwd  *pw;
    static int init;
  
    if (!init)
    {
	/* if the list has yet to be initialized */
	/* zap the list, and rebuild from scratch */
	homehead=NULL;
	hometail=NULL;
	init++;

	setpwent ();

	while ((pw = getpwent ()))
	    if (!hmalloc (pw))
		break;

	endpwent ();
    }
}


static struct aka *
akalloc (char *id)
{
    register struct aka *p;

    p = (struct aka *) mh_xmalloc (sizeof(*p));

    p->ak_name = getcpy (id);
    p->ak_visible = 0;
    p->ak_addr = NULL;
    p->ak_next = NULL;
    if (akatail != NULL)
	akatail->ak_next = p;
    if (akahead == NULL)
	akahead = p;
    akatail = p;

    return p;
}


static struct home *
hmalloc (struct passwd *pw)
{
    register struct home *p;

    p = (struct home *) mh_xmalloc (sizeof(*p));

    p->h_name = getcpy (pw->pw_name);
    p->h_uid = pw->pw_uid;
    p->h_gid = pw->pw_gid;
    p->h_home = getcpy (pw->pw_dir);
    p->h_shell = getcpy (pw->pw_shell);
    p->h_ngrps = 0;
    p->h_next = NULL;
    if (hometail != NULL)
	hometail->h_next = p;
    if (homehead == NULL)
	homehead = p;
    hometail = p;

    return p;
}


struct home *
seek_home (char *name)
{
    register struct home *hp;
    struct passwd *pw;
    char lname[32];
    unsigned char *c;
    char *c1;

    for (hp = homehead; hp; hp = hp->h_next)
	if (!mh_strcasecmp (name, hp->h_name))
	    return hp;

    /*
     * The only place where there might be problems.
     * This assumes that ALL usernames are kept in lowercase.
     */
    for (c = name, c1 = lname; *c && (c1 - lname < sizeof(lname) - 1); c++, c1++) {
        if (isalpha(*c) && isupper(*c))
	    *c1 = tolower (*c);
	else
	    *c1 = *c;
    }
    *c1 = '\0';
    if ((pw = getpwnam(lname)))
	return(hmalloc(pw));

    return NULL;
}
