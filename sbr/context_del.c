
/*
 * context_del.c -- delete an entry from the context/profile list
 *
 * $Id$
 */

#include <h/mh.h>

/*
 * Delete a key/value pair from the context/profile list.
 * Return 0 if key is found, else return 1.
 */

int
context_del (char *key)
{
    register struct node *np, *pp;

    /* sanity check - check that context has been read */
    if (defpath == NULL)
	adios (NULL, "oops, context hasn't been read yet");

    for (np = m_defs, pp = NULL; np; pp = np, np = np->n_next) {
	if (!strcasecmp (np->n_name, key)) {
	    if (!np->n_context)
		admonish (NULL, "bug: context_del(key=\"%s\")", np->n_name);
	    if (pp)
		pp->n_next = np->n_next;
	    else
		m_defs = np->n_next;
	    free (np->n_name);
	    if (np->n_field)
		free (np->n_field);
	    free ((char *) np);
	    ctxflags |= CTXMOD;
	    return 0;
	}
    }

    return 1;
}
