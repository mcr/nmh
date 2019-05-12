#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "libg.h"

char *getpswd(uid)
{
	static char pswd[9];
	register char *bp, *np;

	bp = u_buf; np = pswd;

	if (getubuf(uid) <= 0) return(NULSTR);
	while(*bp++ != ':');
	--bp;
	while((*np++ = *bp++) != ':');
	*--np = '\0';
	return(pswd);
}
