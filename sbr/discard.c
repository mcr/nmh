
/*
 * discard.c -- discard output on a file pointer
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

#include <termios.h>


void
discard (FILE *io)
{
    if (io == NULL)
	return;

    tcflush (fileno(io), TCOFLUSH);

    /* There used to be an fpurge() here on some platforms, stdio
       hackery on others.  But it didn't seem necessary. */
}

