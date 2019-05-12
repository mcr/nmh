#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "libg.h"

getpath(uid)
{
	static char path[50];
	register char *pp, *bp;
	register int n;

	if (getubuf(uid) <= 0) return(NULSTR);
	pp = path; bp = u_buf;
	for(n=0; n<5; n++) while(*bp++ != ':');
	while((*pp++ = *bp++) != ':');
	*--pp = '\0';
	return(p