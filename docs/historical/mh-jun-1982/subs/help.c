#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"

help(str, swp)
	char *str;
	struct swit *swp;
{
	printf("syntax: %s\n", str);
	printf("  switches are:\n");
	printsw(ALL, swp, "-");
}
