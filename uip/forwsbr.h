/* forwsbr.h -- subroutine to build a draft from a component file
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

int
build_form(char *form, char *digest, int *dat, char *from, char *to,
    char *cc, char *fcc, char *subject, char *inputfile);
