
/*
 * m_maildir.c -- get the path for the mail directory
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

static char mailfold[BUFSIZ];

/*
 * static prototypes
 */
static char *exmaildir (char *);


char *
m_maildir (char *folder)
{
    register char *cp, *ep;

    if ((cp = exmaildir (folder))
	    && (ep = cp + strlen (cp) - 1) > cp
	    && *ep == '/')
	*ep = '\0';

    return cp;
}


char *
m_mailpath (char *folder)
{
    register char *cp;
    char maildir[BUFSIZ];

    if (*folder != '/'
	    && strncmp (folder, CWD, NCWD)
	    && strcmp (folder, DOT)
	    && strcmp (folder, DOTDOT)
	    && strncmp (folder, PWD, NPWD)) {
	strncpy (maildir, mailfold, sizeof(maildir));	/* preserve... */
	cp = getcpy (m_maildir (folder));
	strncpy (mailfold, maildir, sizeof(mailfold));
    } else {
	cp = path (folder, TFOLDER);
    }

    return cp;
}


static char *
exmaildir (char *folder)
{
    register char *cp, *pp;

    /* use current folder if none is specified */
    if (folder == NULL)
	folder = getfolder(1);

    if (!(*folder != '/'
	    && strncmp (folder, CWD, NCWD)
	    && strcmp (folder, DOT)
	    && strcmp (folder, DOTDOT)
	    && strncmp (folder, PWD, NPWD))) {
	strncpy (mailfold, folder, sizeof(mailfold));
	return mailfold;
    }

    cp = mailfold;
    if ((pp = context_find ("path")) && *pp) {
	if (*pp != '/') {
	    sprintf (cp, "%s/", mypath);
	    cp += strlen (cp);
	}
	cp = copy (pp, cp);
    } else {
	cp = copy (path ("./", TFOLDER), cp);
    }
    if (cp[-1] != '/')
	*cp++ = '/';
    strcpy (cp, folder);

    return mailfold;
}
