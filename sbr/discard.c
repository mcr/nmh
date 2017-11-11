/* discard.c -- discard output on a file pointer
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "discard.h"

#include <termios.h>

void
discard (FILE *io)
{
    if (io)
        tcflush (fileno(io), TCOFLUSH);
}
