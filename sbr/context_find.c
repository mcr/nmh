
/*
 * context_find.c -- find an entry in the context/profile list
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


char *
context_find (char *str)
{
    struct node *np;

    for (np = m_defs; np; np = np->n_next)
	if (!mh_strcasecmp (np->n_name, str))
	    return (np->n_field);

    return NULL;
}
