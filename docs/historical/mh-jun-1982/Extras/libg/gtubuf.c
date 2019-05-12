#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "libg.h"

getubuf(uid)
{
	if (uid != last_uid || uid == 0)
		if (getpw(uid,u_buf)) return(0);
	last_uid = uid;
	return(1);
}
