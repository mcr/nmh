
/*
 * context_replace.c -- add/replace an entry in the context/profile list
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>


void
context_replace (char *key, char *value)
{
    register struct node *np;

    /*
     * If list is empty, allocate head of profile/context list.
     */
    if (!m_defs) {
	NEW(np);
        m_defs = np;
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
	if (!strcasecmp (np->n_name ? np->n_name : "", key ? key : "")) {
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
    NEW(np->n_next);
    np = np->n_next;
    np->n_name = getcpy (key);
    np->n_field = getcpy (value);
    np->n_context = 1;
    np->n_next = NULL;
    ctxflags |= CTXMOD;
}
