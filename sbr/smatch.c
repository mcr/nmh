/* smatch.c -- match a switch (option)
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "smatch.h"


int
smatch(const char *string, const struct swit *swp)
{
    const char *sp, *tcp;
    int firstone, len;
    const struct swit *tp;

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
		return tp->swret;	/* exact match */
	}
	if (*sp) {
	    if (*sp != ' ')
		continue;		/* no match */
	    if (*--tcp == '\0')
		return tp->swret;	/* exact match */
	}
	if (firstone == UNKWNSW)
	    firstone = tp->swret;
	else
	    firstone = AMBIGSW;
    }

    return firstone;
}
