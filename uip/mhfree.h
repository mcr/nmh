/* mhfree.h -- routines to free MIME-message data structures.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

void free_content(CT ct);
void free_ctinfo(CT);
void free_encoding(CT, int);
void freects_done(int) NORETURN;

extern CT *cts;
