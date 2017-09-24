/* getarguments.c -- Get the argument vector ready to go.
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>

char **
getarguments (char *invo_name, int argc, char **argv, int check_context)
{
    char *cp = NULL, **ap = NULL, **bp = NULL, **arguments = NULL;
    int n = 0;

    /*
     * Check if profile/context specifies any arguments
     */
    if (check_context && (cp = context_find (invo_name))) {
	cp = mh_xstrdup(cp);		/* make copy    */
	ap = brkstring (cp, " ", "\n");	/* split string */

	/* Count number of arguments split */
	bp = ap;
	while (*bp++)
	    n++;
    }

    arguments = mh_xmalloc ((argc + n) * sizeof(*arguments));
    bp = arguments;

    /* Copy any arguments from profile/context */
    if (ap != NULL && n > 0) {
	while (*ap)
	    *bp++ = *ap++;
     }

    /* Copy arguments from command line */
    argv++;
    while (*argv)
	*bp++ = *argv++;

    /* Now NULL terminate the array */
    *bp = NULL;

    return arguments;
}
