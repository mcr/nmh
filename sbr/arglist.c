/* arglist.c -- Routines for handling argument lists for execvp() and friends
 *
 * This code is Copyright (c) 2013, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "error.h"
#include "h/utils.h"

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
    int i;

    bool space = false;
    bool metachar = false;
    for (p = command; *p; p++) {
    	if (*p == ' ' || *p == '\t') {
		space = true;
	} else if (strchr(METACHARS, *p)) {
		metachar = true;
		break;
	}
    }

    argvarray = mh_xmalloc(sizeof *argvarray * (MAXARGS + 5));

    /*
     * The simple case - no spaces or shell metacharacters
     */

    if (!space && !metachar) {
    	argvarray[0] = mh_xstrdup(r1bindex(command, '/'));
	argvarray[1] = NULL;
	*file = mh_xstrdup(command);
	if (argp)
	    *argp = 1;
	return argvarray;
    }

    /*
     * Spaces, but no shell metacharacters; space-split into separate
     * arguments
     */

    if (space && !metachar) {
    	char **split;
	p = mh_xstrdup(command);
	split = brkstring(p, " \t", NULL);
	if (split[0] == NULL) {
	    die("Invalid blank command found");
	}
	argvarray[0] = mh_xstrdup(r1bindex(split[0], '/'));
	for (i = 1; split[i] != NULL; i++) {
	    if (i > MAXARGS) {
		die("Command exceeded argument limit");
	    }
	    argvarray[i] = mh_xstrdup(split[i]);
	}
	argvarray[i] = NULL;
	*file = mh_xstrdup(split[0]);
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

    *file = mh_xstrdup("/bin/sh");
    argvarray[0] = mh_xstrdup("sh");
    argvarray[1] = mh_xstrdup("-c");
    argvarray[2] = mh_xstrdup(command);
    argvarray[2] = add(" \"$@\"", argvarray[2]);
    argvarray[3] = mh_xstrdup("/bin/sh");
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

    free(command);

    if (argvarray != NULL) {
    	for (i = 0; argvarray[i] != NULL; i++)
	    free(argvarray[i]);
	free(argvarray);
    }
}

/*
 * Similar in functionality to argsplit, but is designed to deal with
 * a msgs_array.
 */

void
argsplit_msgarg(struct msgs_array *msgs, char *command, char **program)
{
    int argp, i;
    char **vec;

    vec = argsplit(command, program, &argp);

    /*
     * As usual, there is lousy memory management in nmh.  Nothing ever
     * free's the msgs_array, and a lot of the arguments are allocated
     * from static memory.  I could have app_msgarg() allocate new
     * memory for each pointer, but for now I decided to stick with
     * the existing interface; maybe that will be revisited later.
     * So we'll just copy over our pointers and free the pointer list
     * (not the actual pointers themselves).  Note that we don't
     * include a trailing NULL, since we are expecting the application
     * to take care of that.
     */

    for (i = 0; i < argp; i++) {
    	app_msgarg(msgs, vec[i]);
    }

    free(vec);
}

/*
 * Insert a arglist vector into the beginning of an struct msgs array
 *
 * Uses by some programs (e.g., show) who want to decide which proc
 * to use after the argument vector has been constructed
 */

#ifndef MAXMSGS
#define MAXMSGS 256
#endif

void
argsplit_insert(struct msgs_array *msgs, char *command, char **program)
{
    int argp, i;
    char **vec;

    vec = argsplit(command, program, &argp);

    /*
     * Okay, we want to shuffle all of our arguments down so we have room
     * for argp number of arguments.  This means we need to know about
     * msgs_array internals.  If that changes, we need to change this
     * code here.
     */

    if (msgs->size + argp >= msgs->max) {
        msgs->max += max(MAXMSGS, argp);
        msgs->msgs = mh_xrealloc(msgs->msgs, msgs->max * sizeof(*msgs->msgs));
    }

    for (i = msgs->size - 1; i >= 0; i--)
    	msgs->msgs[i + argp] = msgs->msgs[i];

    msgs->size += argp;

    /*
     * Now fill in the arguments at the beginning of the vector.
     */

    for (i = 0; i < argp; i++)
    	msgs->msgs[i] = vec[i];

    free(vec);
}
