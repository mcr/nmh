/* ali.c -- list nmh mail aliases
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/addrsbr.h>
#include <h/aliasbr.h>
#include <h/mts.h>
#include <h/utils.h>

#define ALI_SWITCHES \
    X("alias aliasfile", 0, ALIASW) \
    X("noalias", 0, NALIASW) \
    X("list", 0, LISTSW) \
    X("nolist", 0, NLISTSW) \
    X("user", 0, USERSW) \
    X("nouser", 0, NUSERSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(ALI);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(ALI, switches);
#undef X

static int pos = 1;

extern struct aka *akahead;

/*
 * prototypes
 */
static void print_aka (char *, bool, int);
static void print_usr (char *, bool);


int
main (int argc, char **argv)
{
    int i, vecp = 0;
    bool inverted, list, noalias;
    char *cp, **ap, **argp, buf[BUFSIZ];
    /* Really only need to allocate for argc-1, but must allocate at least 1,
       so go ahead and allocate for argc char pointers. */
    char **vec = mh_xmalloc (argc * sizeof (char *)), **arguments;
    struct aka *ak;

    if (nmh_init(argv[0], 1)) { return 1; }

    mts_init ();
    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    inverted = list = noalias = false;
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
		    done (0);
		case VERSIONSW:
		    print_version (invo_name);
		    done (0);

		case ALIASW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((i = alias (cp)) != AK_OK)
			adios (NULL, "aliasing error in %s - %s", cp, akerror (i));
		    continue;
		case NALIASW: 
		    noalias = true;
		    continue;

		case LISTSW: 
		    list = true;
		    continue;
		case NLISTSW: 
		    list = false;
		    continue;

		case USERSW: 
		    inverted = true;
		    continue;
		case NUSERSW: 
		    inverted = false;
		    continue;
	    }
	}

	if (vecp < argc) {
	    vec[vecp++] = cp;
	} else {
	    /* Should never happen, but try to protect against code changes
	       that could allow it. */
	    adios (NULL, "too many arguments");
	}
    }

    if (!noalias) {
	/* allow Aliasfile: profile entry */
	if ((cp = context_find ("Aliasfile"))) {
	    char *dp = NULL;

	    for (ap = brkstring(dp = mh_xstrdup(cp), " ", "\n"); ap && *ap; ap++)
		if ((i = alias (*ap)) != AK_OK)
		    adios (NULL, "aliasing error in %s - %s", *ap, akerror (i));
            mh_xfree(dp);
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
	    print_usr (vec[i], list);
    } else {
	if (vecp) {
	    /* print specified aliases */
	    for (i = 0; i < vecp; i++)
		print_aka (akvalue (vec[i]), list, 0);
	} else {
	    /* print them all */
	    for (ak = akahead; ak; ak = ak->ak_next) {
                char *res;

		printf ("%s: ", ak->ak_name);
		pos += strlen (ak->ak_name) + 1;
                res = akresult(ak);
		print_aka(res, list, pos);
                free(res);
	    }
	}
    }

    free (vec);
    done (0);
    return 1;
}

static void
print_aka (char *p, bool list, int margin)
{
    char c;

    if (p == NULL) {
	puts("<empty>");
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

static void
print_usr (char *s, bool list)
{
    char *cp, *pp, *vp;
    struct aka *ak;
    struct mailname *mp, *np;

    if ((pp = getname (s)) == NULL)
	adios (NULL, "no address in \"%s\"", s);
    if ((mp = getm (pp, NULL, 0, NULL, 0)) == NULL)
	adios (NULL, "bad address \"%s\"", s);
    while (getname (""))
	continue;

    vp = NULL;
    for (ak = akahead; ak; ak = ak->ak_next) {
	pp = akresult (ak);
	while ((cp = getname (pp))) {
	    if ((np = getm (cp, NULL, 0, NULL, 0)) == NULL)
		continue;
	    if (!strcasecmp (FENDNULL(mp->m_host),
			     FENDNULL(np->m_host))  &&
		!strcasecmp (FENDNULL(mp->m_mbox),
			     FENDNULL(np->m_mbox))) {
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

    print_aka (vp ? vp : s, list, 0);

    mh_xfree(vp);
}
