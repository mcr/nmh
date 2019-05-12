#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include <stdio.h>
#include "../mh.h"

char *sprintf();
char *malloc();

char *add(this, that)
	register char *this, *that;
{
	register char *r;

	if(!this)
		this = "";
	if(!that)
		that = "";
	r = malloc((unsigned) (strlen(this)+strlen(that)+1));
	VOID sprintf(r, "%s%s", that, this);
	if(*that)
		cndfree(that);
	return(r);
}
