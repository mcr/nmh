#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/* returns date in format dd-mon-yy@hh:mm:ss\0 */
cdate(dataddr)
long *dataddr;
{
	register char *cp;

	cp = ctime(dataddr);
	cp[1] = cp[8];
	cp[2] = cp[9];
	cp[3] = '-';
	cp[7] = '-';
	cp[8] = cp[22];
	cp[9] = cp[23];
	cp[10] = '@';
	cp[19] = 0;
	return(cp+1);
}
