/* done.c -- terminate the program
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include "error.h"

static void (*altexit)(int) NORETURN = exit;

/* set_done changes the path of done() from exit(3), or back to exit(3).
 * Anything else will work, but a standard-error warning will report the
 * old non-exit() value has been trampled. */
void
set_done(void (*new)(int) NORETURN)
{
    generic_pointer gpo, gpn;

    if (altexit != exit && new != exit) {
        gpo.f = (void (*)(void))altexit;
        gpn.f = (void (*)(void))new;
        inform("altexit trampled: %p %p", gpo.v, gpn.v);
    }
    altexit = new;
}

void NORETURN
done(int status)
{
    (*altexit)(status);
}
