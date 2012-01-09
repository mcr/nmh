
/*
 * signals.c -- general signals interface for nmh
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/signals.h>


/*
 * A version of the function `signal' that uses reliable
 * signals, if the machine supports them.  Also, (assuming
 * OS support), it restarts interrupted system calls for all
 * signals except SIGALRM.
 *
 * Since we are now assuming POSIX signal support everywhere, we always
 * use reliable signals.
 */

SIGNAL_HANDLER
SIGNAL (int sig, SIGNAL_HANDLER func)
{
    struct sigaction act, oact;

    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    if (sig == SIGALRM) {
# ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;     /* SunOS */
# endif
    } else {
# ifdef SA_RESTART
	act.sa_flags |= SA_RESTART;       /* SVR4, BSD4.4 */
# endif
    }
    if (sigaction(sig, &act, &oact) < 0)
	return (SIG_ERR);
    return (oact.sa_handler);
}


/*
 * A version of the function `signal' that will set
 * the handler of `sig' to `func' if the signal is
 * not currently set to SIG_IGN.  Also uses reliable
 * signals if available.
 */
SIGNAL_HANDLER
SIGNAL2 (int sig, SIGNAL_HANDLER func)
{
    struct sigaction act, oact;

    if (sigaction(sig, NULL, &oact) < 0)
	return (SIG_ERR);
    if (oact.sa_handler != SIG_IGN) {
	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (sig == SIGALRM) {
# ifdef SA_INTERRUPT
	    act.sa_flags |= SA_INTERRUPT;     /* SunOS */
# endif
	} else {
# ifdef SA_RESTART
	    act.sa_flags |= SA_RESTART;       /* SVR4, BSD4.4 */
# endif
	}
	if (sigaction(sig, &act, &oact) < 0)
	    return (SIG_ERR);
    }
    return (oact.sa_handler);
}

