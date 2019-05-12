#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../h/iobuf.h"
/* return one line using getc, caller must supply iobuf pointer, */
/* line buffer, and max. count */
/* returns actual char count */

getl(bufp,line,cnt)
struct iobuf *bufp;
char line[];
int cnt;
{
	register char *lp;
	register c, cc;

	lp = line;
	cc = cnt;
	while ((c = getc(bufp)) >= 0) {
		*lp++ = c;
		if (--cc <= 0 || c == '\n') break;
	}
	return(cnt-cc);
}
