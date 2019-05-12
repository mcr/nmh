#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#
#include "../h/errors.h"
#define NSHARGS 25	/* limit on number of args to shell command file  */
execvsh(name,args)
char *name, **args;
{
	extern errno;
	register char **v1, **v2;
	char *shvec[NSHARGS];

	v2 = args;
	for (v1 = v2; *v1; )
		if (*v1++ == -1) *--v1 = 0;
	execv(name, v2);
	/* return 0 if not found, -1 if not executable */
	if (errno == ENOENT || errno == EACCES)
		return (0);
	else if (errno != ENOEXEC)
		return(-1);
	v1 = shvec;
	*v1++ = "sh";
	*v1++ = name;
	*v2++ = "/bin/sh";
	while (*v1++ = *v2++) {
		if (v1 >= &shvec[NSHARGS]) {
			errno = E2BIG;
			return(-1);
		}
	}
	execv(args[0], shvec);
	return(-1);
}

execlsh(name,arg0,arg1,arg2)
char *name, *arg0, *arg1, *arg2;
{
	return(execvsh(name,&arg0));
}
