
/*
 * putenv.c -- (un)set an envariable
 *
 * $Id$
 */

#include <h/mh.h>

extern char **environ;

/*
 * prototypes
 */
int m_putenv (char *, char *);
int unputenv (char *);
static int nvmatch (char *, char *);


int
m_putenv (char *name, char *value)
{
    register int i;
    register char **ep, **nep, *cp;

    if (!(cp = malloc ((size_t) (strlen (name) + strlen (value) + 2))))
	return 1;

    sprintf (cp, "%s=%s", name, value);

    for (ep = environ, i = 0; *ep; ep++, i++)
	if (nvmatch (name, *ep)) {
	    *ep = cp;
	    return 0;
	}

    if (!(nep = (char **) malloc ((size_t) ((i + 2) * sizeof(*nep)))))
	return 1;

    for (ep = environ, i = 0; *ep; nep[i++] = *ep++)
	continue;
    nep[i++] = cp;
    nep[i] = NULL;
    environ = nep;
    return 0;
}


int
unputenv (char *name)
{
    char **ep, **nep;

    for (ep = environ; *ep; ep++)
	if (nvmatch (name, *ep))
	    break;
    if (*ep == NULL)
	return 1;

    for (nep = ep + 1; *nep; nep++)
	continue;
    *ep = *--nep;
    *nep = NULL;
    return 0;
}


static int
nvmatch (char *s1, char *s2)
{
    while (*s1 == *s2++)
	if (*s1++ == '=')
	    return 1;

    return (*s1 == '\0' && *--s2 == '=');
}
