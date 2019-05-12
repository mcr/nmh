#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/*
 * This routine returns the address on the stack of the text of the
 *  first argument to the process.  It only works on the VAX, and only
 *  if the process was not called with 4 empty args in a row.
 *
 *   This is a crock, and it doesn't work on 4bsd.
 *   invo_name is now an external char * pointer
 *   2/1/81 - dave yost.
 */

char *invo_name()
{
	register int *ip;

	ip = (int *) 0x7ffffff8;        /* Highest stack address -4 */

	while(*--ip != 0)               /* Look backwards for bumber */
		continue;
	return (char *) &ip[1];         /* Next string is i