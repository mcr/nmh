#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

type(ch, s)
char *s;
{
	register char *p;

	for (p = s; *p++; );
	--p;
	write(ch, s, p-s);
	return(p-s);
}
