
/*
 * pidwait.c -- wait for child to exit
 *
 * $Id$
 */

#include <h/mh.h>
#include <h/signals.h>
#include <signal.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

int
pidwait (pid_t id, int sigsok)
{
    pid_t pid;
    sigset_t set, oset;

#ifdef WAITINT
    int status;
#else
    union wait status;
#endif

    if (sigsok == -1) {
	/* block a couple of signals */
	sigemptyset (&set);
	sigaddset (&set, SIGINT);
	sigaddset (&set, SIGQUIT);
	SIGPROCMASK (SIG_BLOCK, &set, &oset);
    }

#ifdef HAVE_WAITPID
    pid = waitpid(id, &status, 0);
#else
    while ((pid = wait(&status)) != -1 && pid != id)
	continue;
#endif

    if (sigsok == -1) {
	/* reset the signal mask */
	SIGPROCMASK (SIG_SETMASK, &oset, &set);
    }

#ifdef WAITINT
    return (pid == -1 ? -1 : status);
#else
    return (pid == -1 ? -1 : status.w_status);
#endif
}
