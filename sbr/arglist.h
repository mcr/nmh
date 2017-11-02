/* arglist.h -- Routines for handling argument lists for execvp() and friends
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

char **argsplit(char *, char **, int *) NONNULL(1, 2);
void arglist_free(char *, char **);
void argsplit_msgarg(struct msgs_array *, char *, char **) NONNULL(1, 2, 3);
void argsplit_insert(struct msgs_array *, char *, char **) NONNULL(1, 2, 3);
