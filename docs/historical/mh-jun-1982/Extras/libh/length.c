#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/* return length of a string, not counting the null terminator */
length(str)
char *str;
{
	register char *cp;

	for (cp = str; *cp++; );
	return(cp-str-1);
}
