
/*
 * context_foil.c -- foil search of profile and context
 *
 * $Id$
 */

#include <h/mh.h>

/*
 * Foil search of users .mh_profile
 * If error, return -1, else return 0
 */

int
context_foil (char *path)
{
    register struct node *np;

    defpath = context = "/dev/null";

    /*
     * If path is given, create a minimal profile/context list
     */
    if (path) {
	if (!(m_defs = (struct node *) malloc (sizeof(*np)))) {
	    advise (NULL, "unable to allocate profile storage");
	    return -1;
	}

	np = m_defs;
	if (!(np->n_name = strdup ("Path"))) {
	    advise (NULL, "strdup failed");
	    return -1;
	}
	if (!(np->n_field = strdup (path))) {
	    advise (NULL, "strdup failed");
	    return -1;
	}
	np->n_context = 0;
	np->n_next = NULL;

	if (mypath == NULL && (mypath = getenv ("HOME")) != NULL)
	    if (!(mypath = strdup (mypath))) {
		advise (NULL, "strdup failed");
		return -1;
	    }
    }

    return 0;
}

