
/*
 * getanswer.c -- get a yes/no answer from the user
 */

#include <h/mh.h>
#include <stdio.h>


int
getanswer (char *prompt)
{
    static int interactive = -1;

    if (interactive < 0)
	interactive = isatty (fileno (stdin)) ? 1 : 0;

    return (interactive ? gans (prompt, anoyes) : 1);
}
