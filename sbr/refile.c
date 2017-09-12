/* refile.c -- call the "fileproc" to refile the
 *          -- msg or draft into another folder
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>


int
refile (char **arg, char *file)
{
    pid_t pid;
    int vecp;
    char **vec;
    char *program;

    vec = argsplit(fileproc, &program, &vecp);

    vec[vecp++] = mh_xstrdup("-nolink"); /* override bad .mh_profile defaults */
    vec[vecp++] = mh_xstrdup("-nopreserve");
    vec[vecp++] = mh_xstrdup("-file");
    vec[vecp++] = getcpy(file);

    if (arg) {
	while (*arg)
	    vec[vecp++] = mh_xstrdup(*arg++);
    }
    vec[vecp] = NULL;

    context_save();	/* save the context file */
    fflush(stdout);

    switch (pid = fork()) {
	case -1: 
	    advise ("fork", "unable to");
	    return -1;

	case 0: 
	    execvp (program, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (fileproc);
	    _exit (-1);

	default: 
	    arglist_free(program, vec);
	    return pidwait(pid, -1);
    }
}
