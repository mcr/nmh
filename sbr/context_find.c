/* context_find.c -- find an entry in the context/profile list
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


char *
context_find (const char *str)
{
    struct node *np;

    str = FENDNULL(str);
    for (np = m_defs; np; np = np->n_next)
	if (!strcasecmp(FENDNULL(np->n_name), str))
	    return (np->n_field);

    return NULL;
}


/*
 * Helper function to search first, if subtype is non-NULL, for
 * invoname-string-type/subtype and then, if not yet found,
 * invoname-string-type.  If entry is found but is empty, it is
 * treated as not found.
 */
char *
context_find_by_type (const char *string, const char *type,
                      const char *subtype) {
    char *value = NULL;

    if (subtype) {
        char *cp;

        cp = concat (invo_name, "-", string, "-", type, "/", subtype, NULL);
        if ((value = context_find (cp)) != NULL && *value == '\0') value = NULL;
        free (cp);
    }

    if (value == NULL) {
        char *cp;

        cp = concat (invo_name, "-", string, "-", type, NULL);
        if ((value = context_find (cp)) != NULL && *value == '\0') value = NULL;
        free (cp);
    }

    return value;
}


/*
 * Helper function to search profile an entry with name beginning with prefix.
 * The search is case insensitive.
 */
int
context_find_prefix (const char *prefix) {
    struct node *np;
    size_t len;

    len = strlen(prefix);
    for (np = m_defs; np; np = np->n_next) {
	if (np->n_name  &&  ! strncasecmp (np->n_name, prefix, len)) {
	    return 1;
        }
    }

    return 0;
}
