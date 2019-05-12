#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include <stdio.h>

type(ch, s)
char *s;
{
	register char *p;

	for (p = s; *p++; );
	--p;
	if(write(ch, s, p-s) != p-s) {
		fprintf(stderr, "type: write error!");
		perror("");
		return 0;
	}
	return(p-s);
}
