
/*
 * ambigsw.c -- report an ambiguous switch
 *
 * $Id$
 */

#include <h/mh.h>


void
ambigsw (char *arg, struct swit *swp)
{
    advise (NULL, "-%s ambiguous.  It matches", arg);
    print_sw (arg, swp, "-");
}
