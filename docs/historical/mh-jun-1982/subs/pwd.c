#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include <stdio.h>
#include <strings.h>

extern FILE *popen ();

char *
pwd()
{
	register FILE *pp;
	static char curpath[128];

	if((pp = popen("pwd", "r")) == NULL ||
	    fgets(curpath, sizeof curpath, pp) == NULL ||
	    pclose(pp) != 0) {
		fprintf(stderr, "Can't find current directory!\n");
		done(1);
	}
	*rindex(curpath, '\n') = 0;     /* Zap the lf */
	return curpath;
}
