
/*
 * ali.c -- list nmh mail aliases
 *
 * $Id$
 */

#include <h/mh.h>
#include <h/addrsbr.h>
#include <h/aliasbr.h>
#include <mts/generic/mts.h>

/*
 * maximum number of names
 */
#define	NVEC 50

static struct swit switches[] = {
#define	ALIASW                0
    { "alias aliasfile", 0 },
#define	NALIASW               1
    { "noalias", -7 },
#define	LISTSW                2
    { "list", 0 },
#define	NLISTSW               3
    { "nolist", 0 },
#define	NORMSW                4
    { "normalize", 0 },
#define	NNORMSW               5
    { "nonormalize", 0 },
#define	USERSW                6
    { "user", 0 },
#define	NUSERSW               7
    { "nouser", 0 },
#define VERSIONSW             8
    { "version", 0 },
#define	HELPSW                9
    { "help", 0 },
    { NULL, 0 }
};

static int pos = 1;

extern struct aka *akahead;

/*
 * prototypes
 */
void print_aka (char *, int, int);
void print_usr (char *, int, int);


int
main (int argc, char **argv)
{
    int i, vecp = 0, inverted = 0, list = 0;
    int noalias = 0, normalize = AD_NHST;
    char *cp, **ap, **argp, buf[BUFSIZ];
    char *vec[NVEC], **arguments;
    struct aka *ak;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    mts_init (invo_name);
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
		    snprintf (buf, sizeof(buf), "%s [switches] aliases ...",
			invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version (invo_name);
		    done (1);

		case ALIASW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((i = alias (cp)) != AK_OK)
			adios (NULL, "aliasing error in %s - %s", cp, akerror (i));
		    continue;
		case NALIASW: 
		    noalias++;
		    continue;

		case LISTSW: 
		    list++;
		    continue;
		case NLISTSW: 
		    list = 0;
		    continue;

		case NORMSW: 
		    normalize = AD_HOST;
		    continue;
		case NNORMSW: 
		    normalize = AD_NHST;
		    continue;

		case USERSW: 
		    inverted++;
		    continue;
		case NUSERSW: 
		    inverted = 0;
		    continue;
	    }
	}
	vec[vecp++] = cp;
    }

    if (!noalias) {
	/* allow Aliasfile: profile entry */
	if ((cp = context_find ("Aliasfile"))) {
	    char *dp = NULL;

	    for (ap = brkstring(dp = getcpy(cp), " ", "\n"); ap && *ap; ap++)
		if ((i = alias (*ap)) != AK_OK)
		    adios (NULL, "aliasing error in %s - %s", *ap, akerror (i));
	    if (dp)
		free(dp);
	}
	alias (AliasFile);
    }

    /*
     * If -user is specified
     */
    if (inverted) {
	if (vecp == 0)
	    adios (NULL, "usage: %s -user addresses ...  (you forgot the addresses)",
		   invo_name);

	for (i = 0; i < vecp; i++)
	    print_usr (vec[i], list, normalize);

	done (0);
    }

    if (vecp) {
	/* print specified aliases */
	for (i = 0; i < vecp; i++)
	    print_aka (akvalue (vec[i]), list, 0);
    } else {
	/* print them all */
	for (ak = akahead; ak; ak = ak->ak_next) {
	    printf ("%s: ", ak->ak_name);
	    pos += strlen (ak->ak_name) + 1;
	    print_aka (akresult (ak), list, pos);
	}
    }

    return done (0);
}

void
print_aka (char *p, int list, int margin)
{
    char c;

    if (p == NULL) {
	printf ("<empty>\n");
	return;
    }

    while ((c = *p++)) {
	switch (c) {
	    case ',': 
		if (*p) {
		    if (list)
			printf ("\n%*s", margin, "");
		    else {
			if (pos >= 68) {
			    printf (",\n ");
			    pos = 2;
			} else {
			    printf (", ");
			    pos += 2;
			}
		    }
		}

	    case 0: 
		break;

	    default: 
		pos++;
		putchar (c);
	}
    }

    putchar ('\n');
    pos = 1;
}

void
print_usr (char *s, int list, int norm)
{
    register char *cp, *pp, *vp;
    register struct aka *ak;
    register struct mailname *mp, *np;

    if ((pp = getname (s)) == NULL)
	adios (NULL, "no address in \"%s\"", s);
    if ((mp = getm (pp, NULL, 0, norm, NULL)) == NULL)
	adios (NULL, "bad address \"%s\"", s);
    while (getname (""))
	continue;

    vp = NULL;
    for (ak = akahead; ak; ak = ak->ak_next) {
	pp = akresult (ak);
	while ((cp = getname (pp))) {
	    if ((np = getm (cp, NULL, 0, norm, NULL)) == NULL)
		continue;
	    if (!strcasecmp (mp->m_host, np->m_host)
		    && !strcasecmp (mp->m_mbox, np->m_mbox)) {
		vp = vp ? add (ak->ak_name, add (",", vp))
		    : getcpy (ak->ak_name);
		mnfree (np);
		while (getname (""))
		    continue;
		break;
	    }
	    mnfree (np);
	}
    }
    mnfree (mp);

#if 0
    printf ("%s: ", s);
    print_aka (vp ? vp : s, list, pos += strlen (s) + 1);
#else
    print_aka (vp ? vp : s, list, 0);
#endif

    if (vp)
	free (vp);
}
