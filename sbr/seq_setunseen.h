/* seq_setunseen.h -- add/delete all messages which have the SELECT_UNSEEN
 *                 -- bit set to/from the Unseen-Sequence
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

void seq_setunseen(struct msgs *, int);
