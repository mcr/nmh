
/*
 * peekc.c -- peek at the next character in a stream
 *
 * $Id$
 */

#include <h/mh.h>


int
peekc(FILE *fp)
{
    register int c;

    c = getc(fp);
    ungetc(c, fp);
    return c;
}
