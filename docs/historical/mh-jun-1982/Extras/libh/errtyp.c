#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

errtype(file, errmsg)
char *file, *errmsg;
{
	register char *r1, *r2;
	char errlin[80];
	register char *r3;

	r3 = errlin;
	r1 = r3;
	r2 = file;
	while (*r1++ = *r2++);
	r1[-1] = ':';
	r2 = errmsg;
	while (*r1++ = *r2++);
	r1[-1] = '\n';
	write(2, r3, r1-r3);
}
