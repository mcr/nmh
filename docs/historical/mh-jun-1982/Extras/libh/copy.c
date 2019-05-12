#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

copy(from,to)
char *from, *to;
{
	register char *rfrom, *rto;

	rfrom = from;
	rto = to;
	while(*rto++ = *rfrom++);
	return(rto-1);
};
