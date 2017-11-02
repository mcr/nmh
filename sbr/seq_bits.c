/* seq_bits.c -- return the snprintb() string for a sequence
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "seq_bits.h"


char *
seq_bits (struct msgs *mp)
{
    size_t i;
    size_t len;
    static char buffer[BUFSIZ];

    strncpy (buffer, MBITS, sizeof(buffer));

    for (i = 0; i < svector_size (mp->msgattrs); i++) {
	len = strlen (buffer);
	snprintf (buffer + len, sizeof(buffer) - len,
		  "%c%s", FFATTRSLOT + 1 + (int) i,
                  svector_at (mp->msgattrs, i));
	/*
	 * seq_bits() is used by seq_printdebug(), which passes an
	 * unsigned int bit vector to snprintb().  So limit the
	 * size of the return value accordingly.   Even worse,
	 * snprintb() only uses characters up through ASCII space
	 * as the delimiter (the %c character above).  So limit
	 * based on that.
	 */
	if ((int) i == ' ' - FFATTRSLOT - 1) break;
    }

    return buffer;
}
