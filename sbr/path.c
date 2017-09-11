/* path.c -- return a pathname
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include "m_maildir.h"

#define	CWD	"./"
#define	DOT	"."
#define	DOTDOT	".."
#define	PWD	"../"

static char *pwds;

/*
 * static prototypes
 */
static char *expath(char *,int);
static void compath(char *);


/* Return value must be free(3)'d. */
char *
pluspath(char *name)
{
	return path(name + 1, *name == '+' ? TFOLDER : TSUBCWF);
}


/* Return value must be free(3)'d. */
char *
path(char *name, int flag)
{
    char *p, *last;

    p = expath(name, flag);
    last = p + strlen(p) - 1;
    if (last > p && *last == '/')
	*last = '\0';

    return p;
}


/* Return value must be free(3)'d. */
static char *
expath (char *name, int flag)
{
    char *cp, *ep;
    char buffer[BUFSIZ];

    if (flag == TSUBCWF) {
	snprintf (buffer, sizeof(buffer), "%s/%s", getfolder (1), name);
	name = m_mailpath (buffer);
	compath (name);
	snprintf (buffer, sizeof(buffer), "%s/", m_maildir (""));
	if (ssequal (buffer, name)) {
	    cp = name;
	    name = mh_xstrdup(name + strlen(buffer));
	    free (cp);
	}
	flag = TFOLDER;
    }

    if (*name == '/'
	    || (flag == TFOLDER
		&& (!has_prefix(name, CWD)
		    && strcmp (name, DOT)
		    && strcmp (name, DOTDOT)
		    && !has_prefix(name, PWD))))
	return mh_xstrdup(name);

    if (pwds == NULL)
	pwds = pwd ();

    if (strcmp (name, DOT) == 0 || strcmp (name, CWD) == 0)
	return mh_xstrdup(pwds);

    ep = pwds + strlen (pwds);
    if ((cp = strrchr(pwds, '/')) == NULL)
	cp = ep;
    else if (cp == pwds)
        cp++;

    if (has_prefix(name, CWD))
	name += LEN(CWD);

    if (strcmp (name, DOTDOT) == 0 || strcmp (name, PWD) == 0) {
	snprintf (buffer, sizeof(buffer), "%.*s", (int)(cp - pwds), pwds);
	return mh_xstrdup(buffer);
    }

    if (has_prefix(name, PWD))
	name += LEN(PWD);
    else
	cp = ep;

    snprintf (buffer, sizeof(buffer), "%.*s/%s", (int)(cp - pwds), pwds, name);
    return mh_xstrdup(buffer);
}


static void
compath (char *f)
{
    char *cp, *dp;

    if (*f != '/')
	return;

    for (cp = f; *cp;) {
	if (*cp != '/') {
	    cp++;
            continue;
        }

        switch (*++cp) {
            case 0: 
                if (--cp > f)
                    *cp = '\0';
                return;

            case '/': 
                for (dp = cp; *dp == '/'; dp++)
                    continue;
                strcpy (cp--, dp);
                continue;

            case '.': 
                if (strcmp (cp, DOT) == 0) {
                    if (cp > f + 1)
                        cp--;
                    *cp = '\0';
                    return;
                }
                if (strcmp (cp, DOTDOT) == 0) {
                    for (cp -= 2; cp > f; cp--)
                        if (*cp == '/')
                            break;
                    if (cp <= f)
                        cp = f + 1;
                    *cp = '\0';
                    return;
                }
                if (has_prefix(cp, PWD)) {
                    for (dp = cp - 2; dp > f; dp--)
                        if (*dp == '/')
                            break;
                    if (dp <= f)
                        dp = f;
                    strcpy (dp, cp + LEN(PWD) - 1);
                    cp = dp;
                    continue;
                }
                if (has_prefix(cp, CWD)) {
                    strcpy (cp - 1, cp + LEN(CWD) - 1);
                    cp--;
                    continue;
                }
                continue;

            default: 
                cp++;
                continue;
        }
    }
}
