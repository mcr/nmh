/* trimcpy.h -- strip leading and trailing whitespace,
 *           -- replace internal whitespace with spaces,
 *           -- then return a copy.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

char *trimcpy(char *);
char *cpytrim(const char *);
char *rtrim(char *);
