
/*
 * done.c -- terminate the program
 *
 * $Id$
 */

#include <h/mh.h>

int
done (int status)
{
    exit (status);
    return 1;  /* dead code to satisfy the compiler */
}
