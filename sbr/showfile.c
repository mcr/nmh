/* showfile.c -- invoke the `lproc' command
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "context_save.h"
#include "arglist.h"
#include "error.h"


int
showfile (char **arg, char *file)
{
    pid_t pid;
    int vecp;
    char **vec, *program;
    int retval = 1;

    context_save();	/* save the context file */
    fflush(stdout);

    /*
     * If you have your lproc listed as "mhl",
     * then really invoked the mhlproc instead
     * (which is usually mhl anyway).
     */
    if (!strcmp (r1bindex (lproc, '/'), "mhl"))
	lproc = mhlproc;

    switch (pid = fork()) {
    case -1:
	/* fork error */
	advise ("fork", "unable to");
	break;

    case 0:
	/* child */
	vec = argsplit(lproc, &program, &vecp);
	bool isdraft = true;
	if (arg) {
	    while (*arg) {
		if (**arg != '-')
		    isdraft = false;
		vec[vecp++] = *arg++;
	    }
	}
	if (isdraft) {
	    if (!strcmp (vec[0], "show"))
		vec[vecp++] = "-file";
	    vec[vecp++] = file;
	}
	vec[vecp] = NULL;

	execvp (program, vec);
	fprintf (stderr, "unable to exec ");
	perror (lproc);
	_exit(1);

    default:
	/* parent */
	retval = pidwait (pid, -1) & 0377 ? 1 : 0;
    }

    return retval;
}
