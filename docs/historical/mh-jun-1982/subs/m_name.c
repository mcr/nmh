#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"

char *m_name(num)
{
	static char name[4];
	register char *cp;
	register int i;

	name[0] = 0;
	name[1] = 0;
	name[2] = 0;
	name[3] = 0;
	i = num;
	cp = &name[3];
	if(i > 0 && i <= MAXFOLDER)
		do {
			*--cp = (i % 10) + '0';
			i /= 10;
		} while(i);
	else
		*--cp = '?';
	return(cp);
}
