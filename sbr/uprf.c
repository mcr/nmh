
/*
 * uprf.c -- "unsigned" lexical prefix
 *
 * $Id$
 */

#include <h/mh.h>

#define TO_LOWER 040
#define NO_MASK  000


int
uprf (char *c1, char *c2)
{
    int c, mask;

    if (!(c1 && c2))
	return 0;

    while ((c = *c2++))
    {
#ifdef LOCALE
	c &= 0xff;
	mask = *c1 & 0xff;
	c = (isalpha(c) && isupper(c)) ? tolower(c) : c;
	mask = (isalpha(mask) && isupper(mask)) ? tolower(mask) : mask;
	if (c != mask)
#else
	mask = (isalpha(c) && isalpha(*c1)) ?  TO_LOWER : NO_MASK;
	if ((c | mask) != (*c1 | mask))
#endif
	    return 0;
	else
	    c1++;
    }
    return 1;
}
