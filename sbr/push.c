
/*
 * push.c -- push a fork into the background
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/signals.h>
#include <signal.h>


void
push(void)
{
    pid_t pid;
    int i;

    for (i = 0; (pid = fork()) == -1 && i < 5; i++)
	sleep (5);

    switch (pid) {
	case -1:
	    /* fork error */
	    advise (NULL, "unable to fork, so can't push...");
	    break;

	case 0:
	    /* child, block a few signals and continue */
	    SIGNAL (SIGHUP, SIG_IGN);
	    SIGNAL (SIGINT, SIG_IGN);
	    SIGNAL (SIGQUIT, SIG_IGN);
	    SIGNAL (SIGTERM, SIG_IGN);
#ifdef SIGTSTP
	    SIGNAL (SIGTSTP, SIG_IGN);
	    SIGNAL (SIGTTIN, SIG_IGN);
	    SIGNAL (SIGTTOU, SIG_IGN);
#endif
	    freopen ("/dev/null", "r", stdin);
	    freopen ("/dev/null", "w", stdout);
	    break;

	default:
	    /* parent, just exit */
	    done (0);
    }
}

