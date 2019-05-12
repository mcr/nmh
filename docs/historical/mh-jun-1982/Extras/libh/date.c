#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/** Version of date.c in s1 modified to be a subroutine
*  it is called with a string and returns a pointer to ints.
*  ie,		ip = date(string);
*  returns 0 if it fails.
*  only error message is bad conversion if string is illegal.
*  The only legal format is ########[ps]
*  where the int field is fixed input and 'p' forces PM by adding 12 hours
*  and 's' would force standard savings time
*/
int	timbuf[2];
char	*cbp;
int *cpx;
int	dmsize[];
date(string)
char *string;
{
	extern int timezone, *localtime();

	cbp = string;
	if(gtime()) {
		write(2, "bad conversion\n", 15);
		return(0);
	}
	if (*cbp != 's') {
	/* convert to Greenwich time, on assumption of Standard time. */
		dpadd(timbuf, timezone);
	/* Now fix up to local daylight time. */
		if ((cpx = localtime(timbuf))[8])
			dpadd(timbuf, -1*60*60);
	}
	return(timbuf);
}

gtime()
{
	register int i;
	register int y, t;
	int d, h, m;
	extern int *localtime();
	int nt[2];

	t = gpair();
	if(t<1 || t>12)
		goto bad;
	d = gpair();
	if(d<1 || d>31)
		goto bad;
	h = gpair();
	if(h == 24) {
		h = 0;
		d++;
	}
	m = gpair();
	if(m<0 || m>59)
		goto bad;
	y = gpair();
	if (y<0) {
		time(nt);
		y = localtime(nt)[5];
	}
	if (*cbp == 'p')
		h =+ 12;
	if (h<0 || h>23)
		goto bad;
	timbuf[0] = 0;
	timbuf[1] = 0;
	y =+ 1900;
	for(i=1970; i<y; i++)
		gdadd(dysize(i));
	/* Leap year */
	if (dysize(y)==366 && t >= 3)
		gdadd(1);
	while(--t)
		gdadd(dmsize[t-1]);
	gdadd(d-1);
	gmdadd(24, h);
	gmdadd(60, m);
	gmdadd(60, 0);
	return(0);

bad:
	return(1);
}

gdadd(n)
{
	register char *t;

	t = timbuf[1]+n;
	if(t < timbuf[1])
		timbuf[0]++;
	timbuf[1] = t;
}

gmdadd(m, n)
{
	register int t1;

	timbuf[0] =* m;
	t1 = timbuf[1];
	while(--m)
		gdadd(t1);
	gdadd(n);
}

gpair()
{
	register int c, d;
	register char *cp;

	cp = cbp;
	if(*cp == 0)
		return(-1);
	c = (*cp++ - '0') * 10;
	if (c<0 || c>100)
		return(-1);
	if(*cp == 0)
		return(-1);
	if ((d = *cp++ - '0') < 0 || d > 9)
		return(-1);
	cbp = cp;
	return (c+d);
}
