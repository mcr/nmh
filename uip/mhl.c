/* mhl.c -- the nmh message listing program
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include "h/done.h"
#include <h/utils.h>


int
main (int argc, char **argv)
{
    if (nmh_init(argv[0], true, false)) { return 1; }

    done (mhl (argc, argv));
    return 1;
}
