#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include <stdio.h>
#include "../mh.h"

ambigsw(arg, swp)
	char *arg;
	struct swit *swp;
{
	fprintf(stderr, "%s: ", invo_name);
	fprintf(stderr, "-%s ambiguous.  It matches \n", arg);
	printsw(arg, swp, "-");
}

