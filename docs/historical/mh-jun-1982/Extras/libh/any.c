#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

any(chr,stg)
char chr, *stg;
{
	register char c, *s;

	c = chr;
	for (s = stg; *s;)
		if (*s++ == c) return (1);
	return (0);
}
