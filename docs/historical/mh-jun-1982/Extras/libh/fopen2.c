#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../h/iobuf.h"

/* this is the counterpart to fopen which leaves
 * the file opened for both read and write
 * fseek may then be used to position at the end of the
 * file (or wherever) before reading/writing
 */
fopen2(filename, abufp)
char *filename;
struct iobuf *abufp;
{
	register struct iobuf *bufp;

	bufp = abufp;
	bufp->b_nleft = 0;
	bufp->b_nextp = 0;
	if ((bufp->b_fildes = open(filename, 2)) < 0) return(-1);
	return(0);
}
