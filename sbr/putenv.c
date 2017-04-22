/* putenv.c -- (un)set an envariable
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>

extern char **environ;

/*
 * prototypes
 */
int m_putenv (char *, char *);
int unputenv (char *);
static int nvmatch (char *, char *);

/* FIXME: These functions leak memory.  No easy fix since they might not
 * be malloc'd.  Switch to setenv(3) and unsetenv(3). */

int
m_putenv (char *name, char *value)
{
    int i;
    char **ep, **nep, *cp;

    cp = concat(name, "=", value, NULL);

    for (ep = environ, i = 0; *ep; ep++, i++)
	if (nvmatch (name, *ep)) {
	    *ep = cp;
	    return 0;
	}

    nep = (char **) mh_xmalloc ((size_t) ((i + 2) * sizeof(*nep)));

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
