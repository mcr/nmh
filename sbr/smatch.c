
/*
 * smatch.c -- match a switch (option)
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


int
smatch(char *string, struct swit *swp)
{
    char *sp, *tcp;
    int firstone, len;
    struct swit *tp;

    firstone = UNKWNSW;

    if (!string)
	return firstone;
    len = strlen(string);

    for (tp = swp; tp->sw; tp++) {
	tcp = tp->sw;
	if (len < abs(tp->minchars))
	    continue;			/* no match */
	for (sp = string; *sp == *tcp++;) {
	    if (*sp++ == '\0')
		return (tp - swp);	/* exact match */
	}
	if (*sp) {
	    if (*sp != ' ')
		continue;		/* no match */
	    if (*--tcp == '\0')
		return (tp - swp);	/* exact match */
	}
	if (firstone == UNKWNSW)
	    firstone = tp - swp;
	else
	    firstone = AMBIGSW;
    }

    return (firstone);
}
