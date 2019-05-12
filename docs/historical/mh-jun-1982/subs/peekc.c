#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include <stdio.h>
#include "../mh.h"

peekc(ib)
FILE *ib;
{
	register c;

	c = getc(ib);
	VOID ungetc(c,ib);
	return(c);
}
