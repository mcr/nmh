/* trimcpy.c -- strip leading and trailing whitespace,
 *           -- replace internal whitespace with spaces,
 *           -- then return a copy.
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "h/utils.h"


char *
trimcpy (char *cp)
{
    char *sp;

    /* skip over leading whitespace */
    while (isspace((unsigned char) *cp))
	cp++;

    /* start at the end and zap trailing whitespace */
    for (sp = cp + strlen(cp) - 1; sp >= cp; sp--) {
	if (isspace((unsigned char) *sp))
	    *sp = '\0';
	else
	    break;
    }

    /* replace remaining whitespace with spaces */
    for (sp = cp; *sp; sp++) {
	if (isspace((unsigned char) *sp))
	    *sp = ' ';
    }

    /* now return a copy */
    return mh_xstrdup(cp);
}


/*
 * cpytrim() -- return a copy of the argument with:
 *           -- stripped leading and trailing whitespace, and
 *           -- internal whitespace replaced with spaces.
 *
 * This code is Copyright (c) 2013, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */
char *
cpytrim (const char *sp)
{
    char *dp;
    char *cp;

    /* skip over leading whitespace */
    while (isspace ((unsigned char) *sp)) ++sp;

    dp = mh_xstrdup(sp);

    /* start at the end and zap trailing whitespace */
    for (cp = dp + strlen (dp) - 1;
         cp >= dp  &&  isspace ((unsigned char) *cp);
         *cp-- = '\0') continue;

    /* replace remaining whitespace with spaces */
    for (cp = dp; *cp; ++cp) {
        if (isspace ((unsigned char) *cp)) *cp = ' ';
    }

    return dp;
}


/*
 * rtrim() -- modify the argument to:
 *         -- strip trailing whitespace
 *
 * This code is Copyright (c) 2014, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */
char *
rtrim (char *sp)
{
    char *cp;

    /* start at the end and zap trailing whitespace */
    for (cp = sp + strlen (sp) - 1;
         cp >= sp  &&  isspace ((unsigned char) *cp);
         --cp) { continue; }
    *++cp = '\0';

    return sp;
}
