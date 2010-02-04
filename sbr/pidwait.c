
/*
 * pidwait.c -- wait for child to exit
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/signals.h>
#include <errno.h>
#include <signal.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

int
pidwait (pid_t id, int sigsok)
{
    pid_t pid;
    SIGNAL_HANDLER istat = NULL, qstat = NULL;

#ifdef HAVE_UNION_WAIT
    union wait status;
#else
    int status;
#endif

    if (sigsok == -1) {
	/* ignore a couple of signals */
	istat = SIGNAL (SIGINT, SIG_IGN);
	qstat = SIGNAL (SIGQUIT, SIG_IGN);
    }

#ifdef HAVE_WAITPID
    while ((pid = waitpid(id, &status, 0)) == -1 && errno == EINTR)
	;
#else
    while ((pid = wait(&status)) != -1 && pid != id)
	continue;
#endif

    if (sigsok == -1) {
	/* reset the signal handlers */
	SIGNAL (SIGINT, istat);
	SIGNAL (SIGQUIT, qstat);
    }

#ifdef HAVE_UNION_WAIT
    return (pid == -1 ? -1 : status.w_status);
#else
    return (pid == -1 ? -1 : status);
#endif
}
