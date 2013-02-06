
/*
 * trimcpy.c -- strip leading and trailing whitespace,
 *           -- replace internal whitespace with spaces,
 *           -- then return a copy.
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


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
    return getcpy(cp);
}
