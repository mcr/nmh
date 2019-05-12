#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include <stdio.h>

cputc(chr, ip)
register FILE *ip;
{
	if(ip != NULL) {
		putc(chr, ip);
		if(ferror(ip)) {
			perror("Write error");
			done(-1);
		}
	}
}
