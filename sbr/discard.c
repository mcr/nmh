
/*
 * discard.c -- discard output on a file pointer
 *
 * $Id$
 */

#include <h/mh.h>

#ifdef HAVE_TERMIOS_H
# include <termios.h>
#else
# ifdef HAVE_TERMIO_H
#  include <termio.h>
# else
#  include <sgtty.h>
# endif
#endif

#ifdef SCO_5_STDIO
# define _ptr  __ptr
# define _cnt  __cnt
# define _base __base
# define _filbuf(fp)  ((fp)->__cnt = 0, __filbuf(fp))
#endif


void
discard (FILE *io)
{
#ifndef HAVE_TERMIOS_H
# ifdef HAVE_TERMIO_H
    struct termio tio;
# else
    struct sgttyb tio;
# endif
#endif

    if (io == NULL)
	return;

#ifdef HAVE_TERMIOS_H
    tcflush (fileno(io), TCOFLUSH);
#else
# ifdef HAVE_TERMIO_H
    if (ioctl (fileno(io), TCGETA, &tio) != -1)
	ioctl (fileno(io), TCSETA, &tio);
# else
    if (ioctl (fileno(io), TIOCGETP, (char *) &tio) != -1)
	ioctl (fileno(io), TIOCSETP, (char *) &tio);
# endif
#endif

#ifdef _FSTDIO
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

