
/*
 * closefds.c -- close-up fd's
 *
 * $Id$
 */

#include <h/mh.h>


void
closefds(int i)
{
    int nbits = OPEN_MAX;

    for (; i < nbits; i++)
	close (i);
}
