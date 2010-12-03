
/*
 * signals.h -- header file for nmh signal interface
 */

#include <config.h>

/*
 * The type for a signal handler
 */
typedef RETSIGTYPE (*SIGNAL_HANDLER)(int);

/*
 * If not a POSIX machine, then we create our
 * own POSIX style signal sets functions. This
 * currently assumes you have 31 signals, which
 * should be true on most pure BSD machines.
 */
#ifndef POSIX_SIGNALS
# define sigemptyset(s)    (*(s) = 0)
# define sigfillset(s)     (*(s) = ~((sigset_t) 0), 0)
# define sigaddset(s,n)    (*(s) |=  (1 << ((n) - 1)), 0)
# define sigdelset(s,n)    (*(s) &= ~(1 << ((n) - 1)), 0)
# define sigismember(s,n)  ((*(s) & (1 << ((n) - 1))) != 0)
#endif

/*
 * prototypes
 */
int SIGPROCMASK (int, const sigset_t *, sigset_t *);
SIGNAL_HANDLER SIGNAL (int, SIGNAL_HANDLER);
SIGNAL_HANDLER SIGNAL2 (int, SIGNAL_HANDLER);
