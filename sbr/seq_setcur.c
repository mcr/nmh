/* seq_setcur.c -- set the current message ("cur" sequence) for a folder
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "seq_setcur.h"
#include "seq_add.h"


void
seq_setcur (struct msgs *mp, int msgnum)
{
    /*
     * Just call seq_addmsg() to update the
     * "cur" sequence.
     */
    seq_addmsg (mp, current, msgnum, -1, 1);
}
