/* signals.h -- header file for nmh signal interface
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <signal.h>

/*
 * The type for a signal handler
 */
typedef void (*SIGNAL_HANDLER)(int);

/*
 * prototypes
 */
SIGNAL_HANDLER SIGNAL (int, SIGNAL_HANDLER);
SIGNAL_HANDLER SIGNAL2 (int, SIGNAL_HANDLER);
int setup_signal_handlers(void);
