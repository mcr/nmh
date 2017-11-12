/* annosbr.h -- prepend annotation to messages
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

int annotate(char *, char *, char *, bool, bool, int, bool);
void annolist(char *, char *, char *, int);
void annopreserve(int);
