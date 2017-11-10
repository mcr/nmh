/* context_find.h -- find an entry in the context/profile list
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

char *context_find(const char *) PURE;
char *context_find_by_type(const char *, const char *, const char *);
int context_find_prefix(const char *) PURE;
