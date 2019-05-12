#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../h/stat.h"

ttynum(name)
char *name;
{
	/* this routine returns the internal number */
	/* of the given tty name (string) */
	/* if the name="tty" or "", the number is that */
	/* of the user's tty. */
	/* this is obviously very system dependent */
	register int major, minor, flags;
	struct inode ibuf;
	struct { char low, high; };

	if (*name == '\0' || equal(name,"tty")) {
		if (fstat(2,&ibuf) < 0 &&
		    fstat(1,&ibuf) < 0) return(-1);
	}
	else
	if (stat(cat("/dev/",name),&ibuf) < 0 &&
	    stat(cat("/dev/tty",name),&ibuf) < 0) return(-1);
	flags = ibuf.i_mode;
	if ((flags & IFMT) != IFCHR) return(-1);
	minor = ibuf.i_addr[0].low;
	major = ibuf.i_addr[0].high;
	if (major == 0)
		return(minor);
	else
	if (major == 1)
		return(minor+1);
	else
	return (-1);
}
