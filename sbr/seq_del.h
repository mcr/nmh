/* seq_del.h -- delete message(s) from a sequence
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

int seq_delmsg(struct msgs *, char *, int);
int seq_delsel(struct msgs *, char *, int, int);
