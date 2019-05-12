#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

equal(s1,s2)
char *s1, *s2;
{
	register char *r1, *r2;
	r1 = s1; r2 = s2;
	while (*r1++ == *r2)
		if (*r2++ == 0) return (1);
	return(0);
}
