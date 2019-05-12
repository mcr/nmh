#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

char *malloc();
extern char *strcpy();


char *trimcpy(cp)
register char *cp;
{
	register char *sp;

	while(*cp == ' ' || *cp == '\t')
		cp++;

	/* Zap trailing NL */
	cp[strlen(cp) - 1] = '\0';

	/* Replace embedded NL's with blanks */
	for(sp = cp; *sp; sp++)
		if(*sp == '\n')
			*sp = ' ';
	sp = malloc((unsigned)(sp - cp + 1));
	strcpy(sp, cp);
	return(sp);
}
