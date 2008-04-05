
/*
 * path.c -- return a pathname
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

#define	CWD	"./"
#define	NCWD	(sizeof(CWD) - 1)
#define	DOT	"."
#define	DOTDOT	".."
#define	PWD	"../"
#define	NPWD	(sizeof(PWD) - 1)

static char *pwds;

/*
 * static prototypes
 */
static char *expath(char *,int);
static void compath(char *);

char *
pluspath(char *name)
{
	return path(name + 1, *name == '+' ? TFOLDER : TSUBCWF);
}

char *
path(char *name, int flag)
{
    register char *cp, *ep;

    if ((cp = expath (name, flag))
	    && (ep = cp + strlen (cp) - 1) > cp
	    && *ep == '/')
	*ep = '\0';

    return cp;
}


static char *
expath (char *name, int flag)
{
    register char *cp, *ep;
    char buffer[BUFSIZ];

    if (flag == TSUBCWF) {
	snprintf (buffer, sizeof(buffer), "%s/%s", getfolder (1), name);
	name = m_mailpath (buffer);
	compath (name);
	snprintf (buffer, sizeof(buffer), "%s/", m_maildir (""));
	if (ssequal (buffer, name)) {
	    cp = name;
	    name = getcpy (name + strlen (buffer));
	    free (cp);
	}
	flag = TFOLDER;
    }

    if (*name == '/'
	    || (flag == TFOLDER
		&& (strncmp (name, CWD, NCWD)
		    && strcmp (name, DOT)
		    && strcmp (name, DOTDOT)
		    && strncmp (name, PWD, NPWD))))
	return getcpy (name);

    if (pwds == NULL)
	pwds = pwd ();

    if (strcmp (name, DOT) == 0 || strcmp (name, CWD) == 0)
	return getcpy (pwds);

    ep = pwds + strlen (pwds);
    if ((cp = strrchr(pwds, '/')) == NULL)
	cp = ep;
    else
	if (cp == pwds)
	    cp++;

    if (strncmp (name, CWD, NCWD) == 0)
	name += NCWD;

    if (strcmp (name, DOTDOT) == 0 || strcmp (name, PWD) == 0) {
	snprintf (buffer, sizeof(buffer), "%.*s", (int)(cp - pwds), pwds);
	return getcpy (buffer);
    }

    if (strncmp (name, PWD, NPWD) == 0)
	name += NPWD;
    else
	cp = ep;

    snprintf (buffer, sizeof(buffer), "%.*s/%s", (int)(cp - pwds), pwds, name);
    return getcpy (buffer);
}


static void
compath (char *f)
{
    register char *cp, *dp;

    if (*f != '/')
	return;

    for (cp = f; *cp;)
	if (*cp == '/') {
	    switch (*++cp) {
		case 0: 
		    if (--cp > f)
			*cp = '\0';
		    break;

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
			break;
		    }
		    if (strcmp (cp, DOTDOT) == 0) {
			for (cp -= 2; cp > f; cp--)
			    if (*cp == '/')
				break;
			if (cp <= f)
			    cp = f + 1;
			*cp = '\0';
			break;
		    }
		    if (strncmp (cp, PWD, NPWD) == 0) {
			for (dp = cp - 2; dp > f; dp--)
			    if (*dp == '/')
				break;
			if (dp <= f)
			    dp = f;
			strcpy (dp, cp + NPWD - 1);
			cp = dp;
			continue;
		    }
		    if (strncmp (cp, CWD, NCWD) == 0) {
			strcpy (cp - 1, cp + NCWD - 1);
			cp--;
			continue;
		    }
		    continue;

		default: 
		    cp++;
		    continue;
	    }
	    break;
	}
	else
	    cp++;
}
