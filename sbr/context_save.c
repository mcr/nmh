
/*
 * context_save.c -- write out the updated context file
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/*
 * This function used to support setuid/setgid programs by writing
 * the file as the user.  But that code, m_chkids(), was removed
 * because there no longer are setuid/setgid programs in nmh.
 */

#include <h/mh.h>
#include <h/signals.h>

void
context_save (void)
{
    struct node *np;
    FILE *out;
    sigset_t set, oset;
    int failed_to_lock = 0;
    
    /* No context in use -- silently ignore any changes! */
    if (!ctxpath)
       return;

    if (!(ctxflags & CTXMOD))
	return;
    ctxflags &= ~CTXMOD;

    /* block a few signals */
    sigemptyset (&set);
    sigaddset (&set, SIGHUP);
    sigaddset (&set, SIGINT);
    sigaddset (&set, SIGQUIT);
    sigaddset (&set, SIGTERM);
    sigprocmask (SIG_BLOCK, &set, &oset);

    if (!(out = lkfopendata (ctxpath, "w", &failed_to_lock))) {
	if (failed_to_lock) {
	    adios (ctxpath, "failed to lock");
	} else {
	    adios (ctxpath, "unable to write");
	}
    }
    for (np = m_defs; np; np = np->n_next)
	if (np->n_context)
	    fprintf (out, "%s: %s\n", np->n_name, np->n_field);
    lkfclosedata (out, ctxpath);

    sigprocmask (SIG_SETMASK, &oset, &set); /* reset the signal mask */
}
