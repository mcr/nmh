/* m_popen.c -- Interface for a popen() call that redirects the current
 *		process standard output to the popen()d process.
 *
 * This code is Copyright (c) 2014, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/signals.h>
#include "m_popen.h"

static	int m_pid = NOTOK;	/* Process we're waiting for */
static  int sd = NOTOK;		/* Original standard output */

/*
 * Fork a process and redirect our standard output to that process
 */

void
m_popen (char *name, int savestdout)
{
    int pd[2];
    char *file;
    char **arglist;

    if (savestdout && (sd = dup (fileno (stdout))) == NOTOK)
	adios ("standard output", "unable to dup()");

    if (pipe (pd) == NOTOK)
	adios ("pipe", "unable to");

    switch (m_pid = fork()) {
	case NOTOK: 
	    adios ("fork", "unable to");

	case OK: 
	    SIGNAL (SIGINT, SIG_DFL);
	    SIGNAL (SIGQUIT, SIG_DFL);

	    close (pd[1]);
	    if (pd[0] != fileno (stdin)) {
		dup2 (pd[0], fileno (stdin));
		close (pd[0]);
	    }
	    arglist = argsplit(name, &file, NULL);
	    execvp (file, arglist);
	    fprintf (stderr, "unable to exec ");
	    perror (name);
	    _exit(1);

	default: 
	    close (pd[0]);
	    if (pd[1] != fileno (stdout)) {
		dup2 (pd[1], fileno (stdout));
		close (pd[1]);
	    }
    }
}


void
m_pclose (void)
{
    if (m_pid == NOTOK)
	return;

    if (sd != NOTOK) {
	fflush (stdout);
	if (dup2 (sd, fileno (stdout)) == NOTOK)
	    adios ("standard output", "unable to dup2()");

	clearerr (stdout);
	close (sd);
	sd = NOTOK;
    }
    else
	fclose (stdout);

    pidwait (m_pid, OK);
    m_pid = NOTOK;
}
