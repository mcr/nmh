#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

error(s)
char *s;
{
	register char *p;

	flush();
	write(2, "?", 1);
	for (p = s; *p++; );
	--p;
	write(2, s, p-s);
	write(2, "?\n", 2);
	exit(-1);
}
