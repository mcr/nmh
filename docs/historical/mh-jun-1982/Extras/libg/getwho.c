#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "libg.h"
#include "../h/stat.h"
getwho(i)
{
	register char *st;
	struct inode i_buf;

	st = ttyname(i);
	if (st == NULSTR) return(-1);
	stat(cat("/dev/",st),&i_buf);
	return(i_buf.i_uid&0377);
}
