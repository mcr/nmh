
/*
 * copy.c -- copy a string and return pointer to NULL terminator
 *
 * $Id$
 */

#include <h/mh.h>

char *
copy(char *from, char *to)
{
    while ((*to = *from)) {
	to++;
	from++;
    }
    return (to);
}
