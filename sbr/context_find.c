
/*
 * context_find.c -- find an entry in the context/profile list
 *
 * $Id$
 */

#include <h/mh.h>


char *
context_find (char *str)
{
    struct node *np;

    /* sanity check - check that context has been read */
    if (defpath == NULL)
	adios (NULL, "oops, context hasn't been read yet");

    for (np = m_defs; np; np = np->n_next)
	if (!strcasecmp (np->n_name, str))
	    return (np->n_field);

    return NULL;
}
