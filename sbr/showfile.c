
/*
 * showfile.c -- invoke the `lproc' command
 *
 * $Id$
 */

#include <h/mh.h>


int
showfile (char **arg, char *file)
{
    pid_t pid;
    int isdraft, vecp;
    char *vec[MAXARGS];

    context_save();	/* save the context file */
    fflush(stdout);

    /*
     * If you have your lproc listed as "mhl",
     * then really invoked the mhlproc instead
     * (which is usually mhl anyway).
     */
    if (!strcmp (r1bindex (lproc, '/'), "mhl"))
	lproc = mhlproc;

    switch (pid = vfork()) {
    case -1:
	/* fork error */
	advise ("fork", "unable to");
	return 1;

    case 0:
	/* child */
	vecp = 0;
	vec[vecp++] = r1bindex (lproc, '/');
	isdraft = 1;
	if (arg) {
	    while (*arg) {
		if (**arg != '-')
		    isdraft = 0;
		vec[vecp++] = *arg++;
	    }
	}
	if (isdraft) {
	    if (!strcmp (vec[0], "show"))
		vec[vecp++] = "-file";
	    vec[vecp++] = file;
	}
	vec[vecp] = NULL;

	execvp (lproc, vec);
	fprintf (stderr, "unable to exec ");
	perror (lproc);
	_exit (-1);

    default:
	/* parent */
	return (pidwait (pid, -1) & 0377 ? 1 : 0);
    }

    return 1;	/* NOT REACHED */
}
