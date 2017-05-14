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
int unputenv (char *);
static int nvmatch (char *, char *);

/* FIXME: These functions leak memory.  No easy fix since they might not
 * be malloc'd.  Switch to setenv(3) and unsetenv(3). */

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
