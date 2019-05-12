#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/*
 *      putenv(name, arg)
 *      replaces or adds the `name' entry with "name=arg";
 *
 *      Uses malloc() for space for new vector and new entry;
 *
 *      Bruce Borden            January 1980
 *      The Rand Corporation
 */

#include <stdio.h>

extern  char **environ;

putenv(name, arg)
char *name, *arg;
{
	register int i;
	register char **ep, **nep, *cp;

	if((cp = (char *) malloc(strlen(name) + strlen(arg) + 2)) == NULL)
		return(1);
	strcpy(cp, name);
	strcat(cp, "=");
	strcat(cp, arg);
	for(ep = environ, i = 0; *ep; ep++, i++)
		if(nvmatch(name, *ep)) {
			*ep = cp;
			return(0);
		}
	if((nep = (char **) malloc((i+2) * sizeof *nep)) == NULL)
		return(1);
	for(ep = environ, i = 0; *ep; )
		nep[i++] = *ep++;
	nep[i++] = cp;
	nep[i] = 0;
	environ = nep;
	return(0);
}

/*
 *	s1 is either name, or name=value
 *	s2 is name=value
 *	if names match, return value of s2, else NULL
 *	used for environment searching: see getenv
 */

static
nvmatch(s1, s2)
register char *s1, *s2;
{

	while (*s1 == *s2++)
		if (*s1++ == '=')
			return(1);
	if (*s1 == '\0' && *(s2-1) == '=')
		return(1);
	return(0);
}
