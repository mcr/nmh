#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

ssequal(substr, str)
char *substr, *str;
{
	while(*substr)
		if(*substr++ != *str++)
			return(0);
	return(1);
}
