
/*
 * seq_bits.c -- return the snprintb() string for a sequence
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


char *
seq_bits (struct msgs *mp)
{
    int i;
    size_t len;
    static char buffer[BUFSIZ];

    strncpy (buffer, MBITS, sizeof(buffer));

    for (i = 0; mp->msgattrs[i]; i++) {
	len = strlen (buffer);
	snprintf (buffer + len, sizeof(buffer) - len,
		"%c%s", FFATTRSLOT + 1 + i, mp->msgattrs[i]);
    }

    return buffer;
}
