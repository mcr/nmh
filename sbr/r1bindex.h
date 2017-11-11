/* r1bindex.h -- Given a string and a character, return a pointer
 *            -- to the right of the rightmost occurrence of the
 *            -- character.  If the character doesn't occur, the
 *            -- pointer will be at the beginning of the string.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

char *r1bindex(char *, int) PURE;
