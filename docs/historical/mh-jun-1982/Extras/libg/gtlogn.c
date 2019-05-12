#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "libg.h"

char *getlogn(uid)
{
	static char name[20];
	register char *np, *bp;

	if (getubuf(uid)<=0) return(NULSTR);

	np = name; bp = u_buf;
	while( (*np++ = *bp++) != ':');
	*--np = '\0';
	return(name);
}
