
/*
 * refile.c -- call the "fileproc" to refile the
 *          -- msg or draft into another folder
 *
 * $Id$
 */

#include <h/mh.h>


int
refile (char **arg, char *file)
{
    pid_t pid;
    register int vecp;
    char *vec[MAXARGS];

    vecp = 0;
    vec[vecp++] = r1bindex (fileproc, '/');
    vec[vecp++] = "-nolink";	/* override bad .mh_profile defaults */
    vec[vecp++] = "-nopreserve";
    vec[vecp++] = "-file";
    vec[vecp++] = file;

    if (arg) {
	while (*arg)
	    vec[vecp++] = *arg++;
    }
    vec[vecp] = NULL;

    context_save();	/* save the context file */
    fflush(stdout);

    switch (pid = vfork()) {
	case -1: 
	    advise ("fork", "unable to");
	    return -1;

	case 0: 
	    execvp (fileproc, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (fileproc);
	    _exit (-1);

	default: 
	    return (pidwait (pid, -1));
    }
}
