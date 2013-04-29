
/*
 * getanswer.c -- get a yes/no answer from the user
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


int
getanswer (char *prompt)
{
    static int interactive = -1;

    if (interactive < 0)
	interactive = isatty (fileno (stdin)) ? 1 : 0;

    return (interactive ? gans (prompt, anoyes) : 1);
}
