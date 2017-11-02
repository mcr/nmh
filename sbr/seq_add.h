/* seq_add.h -- add message(s) to a sequence
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

int seq_addsel(struct msgs *, char *, int, int);
int seq_addmsg(struct msgs *, char *, int, int, int);
