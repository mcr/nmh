/* push.c -- push a fork into the background
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/signals.h>
#include "m_mktemp.h"


void
push(void)
{
    pid_t pid;

    pid = fork();
    switch (pid) {
	case -1:
	    /* fork error */
	    inform("unable to fork, so can't push...");
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

	    unregister_for_removal(0);

	    if (freopen ("/dev/null", "r", stdin) == NULL) {
		advise ("stdin", "freopen");
            }
	    if (freopen ("/dev/null", "w", stdout) == NULL) {
		advise ("stdout", "freopen");
            }
	    break;

	default:
	    /* parent, just exit */
	    done (0);
    }
}
