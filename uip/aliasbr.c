/* aliasbr.c -- new aliasing mechanism
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/concat.h"
#include "sbr/vfgets.h"
#include "sbr/getcpy.h"
#include "h/aliasbr.h"
#include "h/addrsbr.h"
#include "h/utils.h"
#include <pwd.h>

static int akvis;
static char *akerrst;

struct aka *akahead = NULL;
struct aka *akatail = NULL;

/*
 * prototypes
 */
static  char *akval (struct aka *, char *);
static bool aleq (char *, char *) PURE;
static char *scanp (char *) PURE;
static char *getp (char *);
static char *seekp (char *, char *, char **);
static int addfile (struct aka *, char *);
static char *getalias (char *);
static void add_aka (struct aka *, char *);
static struct aka *akalloc (char *);


/* Do mh alias substitution on 's' and return the results. */
char *
akvalue (char *s)
{
    char *v;

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
    char *cp = NULL, *dp, *pp;
    struct adr *ad;

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

    /* It'd be tempting to check for a trailing semicolon and remove
       it.  But that would break the EXMH alias parser on what would
       then be valid expressions:
       http://lists.gnu.org/archive/html/nmh-workers/2012-10/msg00039.html
     */

    for (; ak; ak = ak->ak_next) {
	if (aleq (s, ak->ak_name)) {
	    return akresult (ak);
	}

        if (strchr (s, ':')) {
	    /* The first address in a blind list will contain the
	       alias name, so try to match, but just with just the
	       address (not including the list name).  If there's a
	       match, then replace the alias part with its
	       expansion. */

	    char *name = getname (s);
	    char *cp = NULL;

	    if (name) {
		/* s is of the form "Blind list: address".  If address
		   is an alias, expand it. */
		struct mailname *mp = getm (name, NULL, 0, NULL, 0);

		if (mp	&&  mp->m_ingrp) {
		    char *gname = mh_xstrdup(FENDNULL(mp->m_gname));

                    /* FIXME: gname must be true;  add() never returns NULL.
		     * Is some other test required? */
		    if (gname  &&  aleq (name, ak->ak_name)) {
			/* Will leak cp. */
			cp = concat (gname, akresult (ak), NULL);
			free (gname);
		    }
		}

		mnfree (mp);
	    }

	    /* Need to flush getname after use. */
	    while (getname ("")) continue;

	    if (cp) {
		return cp;
	    }
	}
    }

    return mh_xstrdup(s);
}


static bool
aleq (char *string, char *aliasent)
{
    char c;

    while ((c = *string++)) {
	if (*aliasent == '*')
	    return true;
        if (tolower((unsigned char)c) != tolower((unsigned char)*aliasent))
            return false;
        aliasent++;
    }

    return *aliasent == 0 || *aliasent == '*';
}


int
alias (char *file)
{
    int i;
    char *bp, *cp, *pp;
    char lc, *ap;
    struct aka *ak = NULL;
    FILE *fp;

    if (*file != '/'
            && !has_prefix(file, "./") && !has_prefix(file, "../"))
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
		continue;
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
		ak->ak_visible = false;
		break;

	    case ';': 
		ak->ak_visible = true;
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

	default: 
	    snprintf (buffer, sizeof(buffer), "unknown error (%d)", i);
	    break;
    }

    return buffer;
}


static char *
scanp (char *p)
{
    while (isspace ((unsigned char) *p))
	p++;
    return p;
}


static char *
getp (char *p)
{
    char  *cp = scanp (p);

    p = cp;
    while (!isspace ((unsigned char) *cp) && *cp)
	cp++;
    *cp = 0;

    return p;
}


static char *
seekp (char *p, char *c, char **a)
{
    char *cp;

    p = cp = scanp (p);
    while (!isspace ((unsigned char) *cp) && *cp && *cp != ':' && *cp != ';')
	cp++;
    *c = *cp;
    *cp++ = 0;
    *a = cp;

    return p;
}


static int
addfile (struct aka *ak, char *file)
{
    char *cp;
    char buffer[BUFSIZ];
    FILE *fp;

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


static char *
getalias (char *addrs)
{
    char *pp, *qp;
    static char *cp = NULL;

    if (cp == NULL)
	cp = addrs;
    else if (*cp == 0)
        return cp = NULL;

    /* Remove leading any space from the address. */
    for (pp = cp; isspace ((unsigned char) *pp); pp++)
	continue;
    if (*pp == 0)
	return cp = NULL;
    /* Find the end of the address. */
    for (qp = pp; *qp != 0 && *qp != ','; qp++)
	continue;
    /* Set cp to point to the remainder of the addresses. */
    if (*qp == ',')
	*qp++ = 0;
    for (cp = qp, qp--; qp > pp; qp--)
	if (*qp != 0) {
	    if (isspace ((unsigned char) *qp))
		*qp = 0;
	    else
		break;
	}

    return pp;
}


static void
add_aka (struct aka *ak, char *pp)
{
    struct adr *ad, *ld;

    for (ad = ak->ak_addr, ld = NULL; ad; ld = ad, ad = ad->ad_next)
	if (!strcmp (pp, ad->ad_text))
	    return;

    NEW(ad);
    ad->ad_text = mh_xstrdup(pp);
    ad->ad_local = strchr(pp, '@') == NULL && strchr(pp, '!') == NULL;
    ad->ad_next = NULL;
    if (ak->ak_addr)
	ld->ad_next = ad;
    else
	ak->ak_addr = ad;
}


static struct aka *
akalloc (char *id)
{
    struct aka *p;

    NEW(p);
    p->ak_name = getcpy (id);
    p->ak_visible = false;
    p->ak_addr = NULL;
    p->ak_next = NULL;
    if (akatail != NULL)
	akatail->ak_next = p;
    if (akahead == NULL)
	akahead = p;
    akatail = p;

    return p;
}
