/* brkstring.c -- (destructively) split a string into
 *             -- an array of substrings
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "brkstring.h"
#include "h/utils.h"

/* allocate this number of pointers at a time */
#define NUMBROKEN 256

static char **broken = NULL;	/* array of substring start addresses */
static int len = 0;		/* current size of "broken"           */

/*
 * static prototypes
 */
static int brkany (char, char *);


char **
brkstring (char *str, char *brksep, char *brkterm)
{
    int i;
    char c, *s;

    /* allocate initial space for pointers on first call */
    if (!broken) {
	len = NUMBROKEN;
	broken = mh_xmalloc ((size_t) (len * sizeof(*broken)));
    }

    /*
     * scan string, replacing separators with zeroes
     * and enter start addresses in "broken".
     */
    s = str;

    for (i = 0;; i++) {

	/* enlarge pointer array, if necessary */
	if (i >= len) {
	    len += NUMBROKEN;
	    broken = mh_xrealloc (broken, (size_t) (len * sizeof(*broken)));
	}

	while (brkany (c = *s, brksep))
	    *s++ = '\0';

	/*
	 * we are either at the end of the string, or the
	 * terminator found has been found, so finish up.
	 */
	if (!c || brkany (c, brkterm)) {
	    *s = '\0';
	    broken[i] = NULL;
	    break;
	}

	/* set next start addr */
	broken[i] = s;

	while ((c = *++s) && !brkany (c, brksep) && !brkany (c, brkterm))
	    ;	/* empty body */
    }

    return broken;
}


/*
 * If the character is in the string,
 * return 1, else return 0.
 */

static int
brkany (char c, char *str)
{
    return str && c && strchr(str, c);
}
