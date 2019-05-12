#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../h/stat.h"
/* this subr returns owner of tty attached to file handle # 2 */
/* on harv/unix, this may not be the real uid, because */
/* we have the command "alias" which spawns a sub-shell with */
/* a new real uid.  luid (login uid) is used by the harvard shell */
/* to find the "save.txt" file (via getpath(getluid())) for $a - $z */
/* shell macros, and elsewhere by certain accounting routines. */

getluid()
{
	struct inode ibuf;
	if (fstat(2,&ibuf) < 0 &&
	    fstat(1,&ibuf) < 0) return(getruid());
	if ((ibuf.i_mode & IFMT) != IFCHR) return(getruid());
	return(ibuf.i_uid&0377);
}
