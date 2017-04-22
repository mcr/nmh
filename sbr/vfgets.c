/*
 * vfgets.c -- virtual fgets
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>

#define	QUOTE	'\\'


int
vfgets (FILE *in, char **bp)
{
    int toggle;
    char *cp, *dp, *ep, *fp;
    static int len = 0;
    static char *pp = NULL;

    if (pp == NULL)
	pp = mh_xmalloc ((size_t) (len = BUFSIZ));

    for (ep = (cp = pp) + len - 1;;) {
	if (fgets (cp, ep - cp + 1, in) == NULL) {
	    if (cp != pp) {
		*bp = pp;
		return 0;
	    }
	    return (ferror (in) && !feof (in) ? -1 : 1);
	}

	if ((dp = cp + strlen (cp) - 2) < cp || *dp != QUOTE) {
wrong_guess:
	    if (cp > ++dp)
		adios (NULL, "vfgets() botch -- you lose big");
	    if (*dp == '\n') {
		*bp = pp;
		return 0;
	    }
            cp = ++dp;
	} else {
	    for (fp = dp - 1, toggle = 0; fp >= cp; fp--) {
		if (*fp != QUOTE)
		    break;
		else
		    toggle = !toggle;
	    }
	    if (toggle)
		goto wrong_guess;

	    if (*++dp == '\n') {
		*--dp = 0;
		cp = dp;
	    } else {
		cp = ++dp;
	    }
	}

	if (cp >= ep) {
	    int curlen = cp - pp;

	    dp = mh_xrealloc (pp, (size_t) (len += BUFSIZ));
	    cp = dp + curlen;
	    ep = (pp = dp) + len - 1;
	}
    }
}
