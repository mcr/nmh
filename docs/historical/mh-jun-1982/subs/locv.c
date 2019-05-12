#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include <stdio.h>

extern char *sprintf ();

char *
locv(longint)
	long longint;
{
	static char locvbuf[12];

	return sprintf(locvbuf, "%ld", longint);
}
