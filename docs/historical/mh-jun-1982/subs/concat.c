#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
char *malloc();

/*VARARGS*/
char *concat(args)
	char *args;
{
	register char **a;
	register char *cp;
	register unsigned len;
	register char *ret;

	len = 1;
	for(a = &args; *a; )
		len += strlen(*a++);
	ret = cp = malloc(len);
	for(a = &args; *a; )
		cp = copy(*a++, cp);
	return(ret);
}
