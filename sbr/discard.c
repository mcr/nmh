
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

#if defined(_FSTDIO) || defined(__DragonFly__)
    fpurge (io);
#else
# ifdef LINUX_STDIO
    io->_IO_write_ptr = io->_IO_write_base;
# else
    if ((io->_ptr = io->_base))
	io->_cnt = 0;
# endif
#endif
}

