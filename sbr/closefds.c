
/*
 * closefds.c -- close-up fd's
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


void
closefds(int i)
{
    int nbits = OPEN_MAX;

    for (; i < nbits; i++)
	close (i);
}
