/* getline.c -- replacement getline() implementation
 *
 * This code is Copyright (c) 2016, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>

#include "h/mh.h"

/* Largest possible size of buffer that allows SSIZE_MAX to be returned
 * to indicate SSIZE_MAX - 1 characters read before the '\n'.  The
 * additional 1 is for the terminating NUL. */
#define MAX_AVAIL ((size_t)SSIZE_MAX + 1)

/* Ideal increase in size of line buffer. */
#define GROWTH 128

ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    char *l;
    size_t avail;
    size_t used;
    int c;
    bool last;

    if (!lineptr || !n) {
        errno = EINVAL;
        return -1;
    }

    l = *lineptr;
    if (l)
        avail = *n <= MAX_AVAIL ? *n : MAX_AVAIL;
    else
        avail = *n = 0; /* POSIX allows *lineptr = NULL, *n = 42. */
    used = 0;
    last = false;
    for (;;) {
        c = getc(stream);
        if (c == EOF) {
            if (ferror(stream) || /* errno set by stdlib. */
                !used)            /* EOF with nothing read. */
                return -1;
            /* Line will be returned without a \n terminator. */
append_nul:
            c = '\0';
            last = true;
        }

        if (used == avail) {
            size_t want;
            char *new;

            if (avail == MAX_AVAIL) {
                errno = EOVERFLOW;
                return -1;
            }
            want = avail + GROWTH;
            if (want > MAX_AVAIL)
                want = MAX_AVAIL;
            new = realloc(l, want);
            if (!new)
                return -1; /* errno set by stdlib. */
            l = *lineptr = new;
            avail = *n = want;
        }
        l[used++] = c;

        if (last)
            return used - 1; /* Don't include NUL. */

        if (c == '\n')
            goto append_nul; /* Final half loop. */
    }
}
