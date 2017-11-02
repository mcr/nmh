/* pidwait.c -- wait for child to exit
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "h/signals.h"

int
pidwait (pid_t id, int sigsok)
{
    pid_t pid;
    SIGNAL_HANDLER istat = NULL, qstat = NULL;

    int status;

    if (sigsok == -1) {
	/* ignore a couple of signals */
	istat = SIGNAL (SIGINT, SIG_IGN);
	qstat = SIGNAL (SIGQUIT, SIG_IGN);
    }

    while ((pid = waitpid(id, &status, 0)) == -1 && errno == EINTR)
	;

    if (sigsok == -1) {
	/* reset the signal handlers */
	SIGNAL (SIGINT, istat);
	SIGNAL (SIGQUIT, qstat);
    }

    return pid == -1 ? -1 : status;
}
