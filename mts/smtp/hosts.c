
/*
 * hosts.c -- find out the official name of a host
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/*
 * In the SendMail world, we really don't know what the valid
 * hosts are.  We could poke around in the sendmail.cf file, but
 * that still isn't a guarantee.  As a result, we'll say that
 * everything is a valid host, and let SendMail worry about it.
 */

#include <h/mh.h>
#include <h/mts.h>
#include <netdb.h>

static struct host {
    char *h_name;
    char **h_aliases;
    struct host *h_next;
} hosts;


/*
 * static prototypes
 */
static int init_hs(void);


char *
OfficialName (char *name)
{
    char *p, *q, site[BUFSIZ];
    struct hostent *hp;

    static char buffer[BUFSIZ];
    char **r;
    struct host *h;

    for (p = name, q = site; *p && (q - site < sizeof(site) - 1); p++, q++)
	*q = isupper (*p) ? tolower (*p) : *p;
    *q = '\0';
    q = site;

    if (!strcasecmp (LocalName(), site))
	return LocalName();

#ifdef	HAVE_SETHOSTENT
    sethostent (1);
#endif

    if ((hp = gethostbyname (q))) {
	strncpy (buffer, hp->h_name, sizeof(buffer));
	return buffer;
    }
    if (hosts.h_name || init_hs ()) {
	for (h = hosts.h_next; h; h = h->h_next)
	    if (!strcasecmp (h->h_name, q)) {
		return h->h_name;
	    } else {
		for (r = h->h_aliases; *r; r++)
		    if (!strcasecmp (*r, q))
			return h->h_name;
	    }
    }

    strncpy (buffer, site, sizeof(buffer));
    return buffer;
}

/*
 * Use hostable as an exception file for those hosts that aren't
 * on the Internet (listed in /etc/hosts).  These are usually
 * PhoneNet and UUCP sites.
 */

#define	NALIASES 50

static int
init_hs (void)
{
    char  *cp, *dp, **q, **r;
    char buffer[BUFSIZ], *aliases[NALIASES];
    register struct host *h;
    register FILE  *fp;

    if ((fp = fopen (hostable, "r")) == NULL)
	return 0;

    h = &hosts;
    while (fgets (buffer, sizeof(buffer), fp) != NULL) {
	if ((cp = strchr(buffer, '#')))
	    *cp = 0;
	if ((cp = strchr(buffer, '\n')))
	    *cp = 0;
	for (cp = buffer; *cp; cp++)
	    if (isspace (*cp))
		*cp = ' ';
	for (cp = buffer; isspace (*cp); cp++)
	    continue;
	if (*cp == 0)
	    continue;

	q = aliases;
	if ((cp = strchr(dp = cp, ' '))) {
	    *cp = 0;
	    for (cp++; *cp; cp++) {
		while (isspace (*cp))
		    cp++;
		if (*cp == 0)
		    break;
		if ((cp = strchr(*q++ = cp, ' ')))
		    *cp = 0;
		else
		    break;
		if (q >= aliases + NALIASES)
		    break;
	    }
	}

	*q = 0;

	h->h_next = (struct host *) calloc (1, sizeof(*h));
	h = h->h_next;
	h->h_name = getcpy (dp);
	r = h->h_aliases =
		(char **) calloc ((size_t) (q - aliases + 1), sizeof(*q));
	for (q = aliases; *q; q++)
	    *r++ = getcpy (*q);
	*r = 0;
    }

    fclose (fp);
    return 1;
}
