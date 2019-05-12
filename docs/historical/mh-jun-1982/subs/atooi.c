#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif


/* octal version of atoi */
atooi(cp)
register char *cp;
{
	register int i, base;

	i = 0;
	base = 8;
	while(*cp >= '0' && *cp <= '7') {
		i *= base;
		i += *cp++ - '0';
	}
	return(i);
}
