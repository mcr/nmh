#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "libg.h"

/* this routine returns the file name within "/dev" */
/* corresponding to the internal tty #, which */
/* is in turn used by ttynum and getwho */
/* this routine is obviously very system dependent. */

/* ttyname is set up so that, in conjunction */
/* with getwho, the following simple "who" program will work: */
/* for (i = 0; ttnam = ttyname(i); i++)    */
/*     if (luid = getwho(i)) printf("%s %s\n",ttnam,getlogn(luid)); */

/* also, ttynum(ttyname(n)) should equal n if 0 <= n <= 31 */

char *ttyname(num)
{
	register char *tty;
	tty = "ttyx";
	if (num < 0 || num > 31) return(NULSTR);
	if (num == 0) return("tty8");
	if (num < 9) {
		tty[3] = num + ('0' - 1);
		return(tty);
	}
	tty[3] = num - ('a' - 9);
	return(tty);
}
