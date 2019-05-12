#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

makename(prefix,suffix)
char *prefix, *suffix;
{
	static char tmpname[15];
	register char *cp1, *cp2;
	register int pid;

	pid = getpid();
	cp1 = tmpname;
	for (cp2 = prefix; *cp1++ = *cp2++; );
	cp1--;
	do *cp1++ = pid%10 + '0'; while (pid =/ 10);
	for (cp2 = suffix; *cp1++ = *cp2++; );
	if (cp1 > &tmpname + 1) error("strs too long to makename");
	return (tmpname);
}
