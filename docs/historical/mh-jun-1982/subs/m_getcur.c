#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include <stdio.h>
#include "../mh.h"

extern char *sprintf();

m_getcur(name)
	char *name;
{
	register char *cp;
	register int i, j;
	short readonly, curfil = 0;
	char buf[132];

	readonly = (access(".",2) == -1);
	if(readonly) {
		VOID sprintf(buf, "cur-%s", name);
		if(cp = m_find(buf))
			curfil = mu_atoi(cp);
	} else if(i = open(current, 0)) {
		if((j = read(i, buf, sizeof buf)) >= 2){
			buf[j-1] = 0;                   /* Zap <lf> */
			curfil = mu_atoi(buf);
		}
		VOID close(i);
	}
	return curfil;
}
