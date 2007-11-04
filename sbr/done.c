
/*
 * done.c -- terminate the program
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

int (*done) (int) = default_done;

int
default_done (int status)
{
    exit (status);
    return 1;  /* dead code to satisfy the compiler */
}
