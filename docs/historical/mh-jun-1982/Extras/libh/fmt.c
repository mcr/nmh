#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../h/iobuf.h"
#define NBUFFED	256	/* limit on fmt buffer */

extern struct iobuf fout;
int fmtlen;

fmt(fmtstrng, arg1, arg2, arg3)
char *fmtstrng;
{
	int svfildes, svnleft, svnextp;

	flush();
	svfildes = fout.b_fildes;
	svnleft = fout.b_nleft;
	svnextp = fout.b_nextp;

	/* a very illegal file handle */
	fout.b_fildes = 99;
	/* use last half of fout buffer */
	fout.b_nleft = NBUFFED;
	fout.b_nextp = &fout.b_buff[512-NBUFFED];
	printf("%r", &fmtstrng);	/* and pass the buck ... */
	fmtlen = fout.b_nextp - &fout.b_buff[512-NBUFFED];
	putchar(0);

	fout.b_fildes = svfildes;
	fout.b_nleft = svnleft;
	fout.b_nextp = svnextp;

	return(&fout.b_buff[512-NBUFFED]);
}
