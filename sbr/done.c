/* done.c -- terminate the program
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

/* It's undefined behaviour in C99 to convert from a function pointer to
 * a data-object pointer, e.g. void pointer.  gcc's -pedantic warns of
 * this and can stop compilation.  POSIX requires the operation however,
 * e.g. for dlsym(3), and so we know it's safe on POSIX platforms, e.g.
 * the pointers are of the same size.  Thus use a union to subvert gcc's
 * check.  The function-pointer equivalent of a void pointer is any
 * function-pointer type as all function pointers are defined to be
 * convertible from one another;  use the simplest available. */
typedef union {
    void *v;
    void (*f)(void);
} generic_pointer;

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
