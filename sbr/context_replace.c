
/*
 * context_replace.c -- add/replace an entry in the context/profile list
 *
 * $Id$
 */

#include <h/mh.h>


void
context_replace (char *key, char *value)
{
    register struct node *np;

    /* sanity check - check that context has been read */
    if (defpath == NULL)
	adios (NULL, "oops, context hasn't been read yet");

    /*
     * If list is emtpy, allocate head of profile/context list.
     */
    if (!m_defs) {
	if (!(m_defs = (struct node *) malloc (sizeof(*np))))
	    adios (NULL, "unable to allocate profile storage");

	np = m_defs;
	np->n_name = getcpy (key);
	np->n_field = getcpy (value);
	np->n_context = 1;
	np->n_next = NULL;
	ctxflags |= CTXMOD;
	return;
    }

    /*
     * Search list of context/profile entries for
     * this key, and replace its value if found.
     */
    for (np = m_defs;; np = np->n_next) {
	if (!strcasecmp (np->n_name, key)) {
	    if (strcmp (value, np->n_field)) {
		if (!np->n_context)
		    admonish (NULL, "bug: context_replace(key=\"%s\",value=\"%s\")", key, value);
		if (np->n_field)
		    free (np->n_field);
		np->n_field = getcpy (value);
		ctxflags |= CTXMOD;
	    }
	    return;
	}
	if (!np->n_next)
	    break;
    }

    /*
     * Else add this new entry at the end
     */
    np->n_next = (struct node *) malloc (sizeof(*np));
    if (!np->n_next)
	adios (NULL, "unable to allocate profile storage");

    np = np->n_next;
    np->n_name = getcpy (key);
    np->n_field = getcpy (value);
    np->n_context = 1;
    np->n_next = NULL;
    ctxflags |= CTXMOD;
}
