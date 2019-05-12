#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>

extern char *strcpy();

struct  msgs *mp;

m_setcur(num)
{
	char buf[6];
	register int i;
	register char *cp1;

	if(mp->msgflags&READONLY) {
		m_replace(cp1 = concat("cur-",mp->foldpath,0), m_name(num));
		free(cp1);
	} else {
		strcpy(buf, m_name(num));
		cp1 = buf + strlen(buf);
		*cp1++ = '\n';
		if(strcmp(current, "cur"))
			error("\"current\" got Clobbered!! Bug!");
		if((i = creat(current, 0660)) >= 0) {
			if(write(i, buf, cp1-buf) != cp1-buf) {
				fprintf(stderr, "m_setcur: write error on ");
				perror(current);
				done(1);
			}
			VOID close(i);
		}
	}
}
