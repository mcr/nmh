
/*
 * signals.c -- general signals interface for nmh
 *
 * $Id$
 */

#include <h/mh.h>
#include <h/signals.h>


int
SIGPROCMASK (int how, const sigset_t *set, sigset_t *oset)
{
#ifdef POSIX_SIGNALS
    return sigprocmask(how, set, oset);
#else
# ifdef BSD_SIGNALS
    switch(how) {
    case SIG_BLOCK:
	*oset = sigblock(*set);
	break;
    case SIG_UNBLOCK:
	sigfillset(*oset);
	*oset = sigsetmask(*oset);
	sigsetmask(*oset & ~(*set));
	break;
    case SIG_SETMASK:
	*oset = sigsetmask(*set);
	break;
    default:
	adios(NULL, "unknown flag in SIGPROCMASK");
	break;
    }
    return 0;
# endif
#endif
}


/*
 * A version of the function `signal' that uses reliable
 * signals, if the machine supports them.  Also, (assuming
 * OS support), it restarts interrupted system calls for all
 * signals except SIGALRM.
 */

SIGNAL_HANDLER
SIGNAL (int sig, SIGNAL_HANDLER func)
{
#ifdef POSIX_SIGNALS
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
#else
    return signal (sig, func);
#endif
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
#ifdef POSIX_SIGNALS
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
#else
    SIGNAL_HANDLER ofunc;

    if ((ofunc = signal (sig, SIG_IGN)) != SIG_IGN)
	signal (sig, func);
    return ofunc;
#endif
}

