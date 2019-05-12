#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "libg.h"

getunum(name)
char *name;
{
	register char *bp, *np;
	register c;
	int inptr[259];
	int uid;

	if (pich <= 0)
		pich = open("/etc/passwd",0);
	if (pich < 0) return(-1);
	seek(pich,0,0);
	inptr[0] = pich;
	inptr[1] = inptr[2] = 0;
	uid = 0;
	if (u_buf[0] != 0) goto test;  /* first test what is presently in
					 the buffer */

	for(;;) {

		bp = u_buf;
		while((c=getc(inptr)) != '\n') {
			if (c <= 0) return(-1);
			*bp++ = c;
		}
		*bp++ = '\0';

	test:	bp = u_buf;
		np = name;
		while(*bp++ == *np++);
		--bp; --np;
		if (*bp++ == ':' &&
		    *np++ == '\0') {
			while(*bp++ != ':');
			while(*bp != ':')
				uid = uid * 10 + *bp++ - '0';
			last_uid = uid;
			return(uid);
		}
	}
	return(-1);
}
