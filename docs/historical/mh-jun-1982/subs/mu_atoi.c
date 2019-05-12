#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"

mu_atoi(str)
	char *str;
{
	register char *cp;
	register int i;

	i = 0;
	cp = str;
	while(*cp) {
		if(*cp < '0' || *cp > '9')
			return 0;
		i *= 10;
		i += *cp++ - '0';
	}
	if (i > MAXFOLDER)
		return 0;
	return i;
}
