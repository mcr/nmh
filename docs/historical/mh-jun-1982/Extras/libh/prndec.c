#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

prndec(ch, d)
{
	char buf[8];
	register char *cp;
	register int i, j;

	i = d;
	cp = &buf[8];
	*--cp = 0;
	j = 0;
	if(i < 0) {
		i =- i;
		j =+ 10;
	}
	do {
		*--cp = (i % 10) + '0';
		i =/ 10;
		j++;
	} while(i);
	if(j > 10) {
		j =- 9;
		*--cp = '-';
	}
	write(ch, cp, j);
	return(j);
}
