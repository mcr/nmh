/* seq_print.c -- Routines to print sequence information.
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "seq_list.h"
#include "seq_getnum.h"
#include "seq_print.h"

/*
 * Print all the sequences in a folder
 */
void
seq_printall (struct msgs *mp)
{
    size_t i;
    char *list;

    for (i = 0; i < svector_size (mp->msgattrs); i++) {
	list = seq_list (mp, svector_at (mp->msgattrs, i));
	printf ("%s%s: %s\n", svector_at (mp->msgattrs, i),
	    is_seq_private (mp, i) ? " (private)" : "", FENDNULL(list));
    }
}


/*
 * Print a particular sequence in a folder
 */
void
seq_print (struct msgs *mp, char *seqname)
{
    int i;
    char *list;

    /* get the index of sequence */
    i = seq_getnum (mp, seqname);

    /* get sequence information */
    list = seq_list (mp, seqname);

    printf ("%s%s: %s\n", seqname,
	(i == -1) ? "" : is_seq_private(mp, i) ? " (private)" : "", FENDNULL(list));
}
