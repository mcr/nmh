
/*
 * ambigsw.c -- report an ambiguous switch
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


void
ambigsw (char *arg, struct swit *swp)
{
    advise (NULL, "-%s ambiguous.  It matches", arg);
    print_sw (arg, swp, "-", stderr);
}
