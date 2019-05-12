#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"

char *malloc();
char *strcpy();

char *
getcpy(str)
	char *str;
{
	register char *cp;

	cp = malloc((unsigned)strlen(str) + 1);
	VOID strcpy(cp, str);
	return(cp);
}
