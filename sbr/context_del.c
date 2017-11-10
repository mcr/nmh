/* context_del.c -- delete an entry from the context/profile list
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "context_del.h"
#include "error.h"
#include "h/utils.h"

/*
 * Delete a key/value pair from the context/profile list.
 * Return 0 if key is found, else return 1.
 */

int
context_del (char *key)
{
    struct node *np, *pp;

    key = FENDNULL(key);
    for (np = m_defs, pp = NULL; np; pp = np, np = np->n_next) {
	if (!strcasecmp(FENDNULL(np->n_name), key)) {
	    if (!np->n_context)
		inform("bug: context_del(key=\"%s\"), continuing...", np->n_name);
	    if (pp)
		pp->n_next = np->n_next;
	    else
		m_defs = np->n_next;
	    free (np->n_name);
            free(np->n_field);
	    free(np);
	    ctxflags |= CTXMOD;
	    return 0;
	}
    }

    return 1;
}
