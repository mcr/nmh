
/*
 * peekc.c -- peek at the next character in a stream
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


int
peekc(FILE *fp)
{
    int c;

    c = getc(fp);
    ungetc(c, fp);
    return c;
}
