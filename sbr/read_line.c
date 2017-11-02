/* read_line.c -- read a possibly incomplete line from stdin.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "h/utils.h"
#include "read_line.h"

/*
 * Flush standard output, read a line from standard input into a static buffer,
 * zero out the newline, and return a pointer to the buffer.
 * On error, return NULL.
 */
const char *
read_line(void)
{
    static char line[BUFSIZ];

    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) == NULL)
            return NULL;
    trim_suffix_c(line, '\n');

    return line; /* May not be a complete line. */
}
