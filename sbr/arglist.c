
/*
 * arglist.c -- Routines for handling argument lists for execvp() and friends
 *
 * This code is Copyright (c) 2013, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>

/*
 * Split up a command into an appropriate array to pass to execvp()
 * or similar function.  Returns an allocated argv[] array.
 *
 * Function arguments:
 *
 * command - String to split up
 * file    - the first argument to "command", suitable for the first argument
 *           to execvp().  Returns allocated memory that must be free()d.
 * argp    - Index to last element (NULL) of returned argv[] array.
 *
 * Our basic algorithm is this:
 *
 * - If there are no spaces or shell metacharacters in "command", then
 *   take it as-is.
 * - If there are spaces in command, space-split command string.
 * - If we have shell metacharacters, run the command using
 *   /bin/sh -c 'command "$@"'.  In this case, any additional arguments
 *   appended to the arglist will be expanded by "$@".
 *
 * In all cases additional arguments can be added to the argv[] array.
 */

/* Shell metacharacters we use to trigger a call to the shell */

#define METACHARS	"$&*(){}[]'\";\\|?<>~`\n"

char **
argsplit(char *command, char **file, int *argp)
{
    char **argvarray, *p;
    int space = 0, metachar = 0, i;

    for (p = command; *p; p++) {
    	if (*p == ' ' || *p == '\t') {
		space = 1;
	} else if (strchr(METACHARS, *p)) {
		metachar = 1;
		break;
	}
    }

    argvarray = (char **) mh_xmalloc((sizeof(char **) * (MAXARGS + 5)));

    /*
     * The simple case - no spaces or shell metacharacters
     */

    if (!space && !metachar) {
    	argvarray[0] = getcpy(r1bindex(command, '/'));
	argvarray[1] = NULL;
	*file = getcpy(command);
	if (argp)
	    *argp = 1;
	return argvarray;
    }

    /*
     * Spaces, but no shell metacharacters; space-split into seperate
     * arguments
     */

    if (space && !metachar) {
    	char **split;
	p = getcpy(command);
	split = brkstring(p, " \t", NULL);
	if (split[0] == NULL) {
	    adios(NULL, "Invalid blank command found");
	}
	argvarray[0] = getcpy(r1bindex(split[0], '/'));
	for (i = 1; split[i] != NULL; i++) {
	    if (i > MAXARGS) {
		adios(NULL, "Command exceeded argument limit");
	    }
	    argvarray[i] = getcpy(split[i]);
	}
	argvarray[i] = NULL;
	*file = getcpy(split[0]);
	if (argp)
	    *argp = i;
	free(p);
	return argvarray;
    }

    /*
     * Remaining option - pass to the shell.
     *
     * Some notes here:
     *
     * - The command passed to "sh -c" is actually:
     *      command "$@"
     *
     *   If there are additional arguments they will be expanded by the
     *   shell, otherwise "$@" expands to nothing.
     *
     * - Every argument after the -c string gets put into positional
     *   parameters starting at $0, but $@ starts expanding with $1.
     *   So we put in a dummy argument (we just use /bin/sh)
     */

    *file = getcpy("/bin/sh");
    argvarray[0] = getcpy("sh");
    argvarray[1] = getcpy("-c");
    argvarray[2] = getcpy(command);
    argvarray[2] = add(" \"$@\"", argvarray[2]);
    argvarray[3] = getcpy("/bin/sh");
    argvarray[4] = NULL;

    if (argp)
    	*argp = 4;

    return argvarray;
}

/*
 * Free our argument array
 */

void
arglist_free(char *command, char **argvarray)
{
    int i;

    if (command != NULL)
    	free(command);

    if (argvarray != NULL) {
    	for (i = 0; argvarray[i] != NULL; i++)
	    free(argvarray[i]);
	free(argvarray);
    }
}
