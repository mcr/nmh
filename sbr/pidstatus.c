/* pidstatus.c -- report child's status
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

#ifndef WTERMSIG
# define WTERMSIG(s) ((int)((s) & 0x7F))
#endif

#ifndef WCOREDUMP
# define WCOREDUMP(s) ((s) & 0x80)
#endif

/*
 * Return 0 if the command exited with an exit code of zero, a nonzero code
 * otherwise.
 *
 * Print out an appropriate status message we didn't exit with an exit code
 * of zero.
 */

int
pidstatus (int status, FILE *fp, char *cp)
{
    int signum;
    char *signame;

/*
 * I have no idea what this is for (rc)
 * so I'm commenting it out for right now.
 *
 *  if ((status & 0xff00) == 0xff00)
 *      return status;
 */

    /* If child process returned normally */
    if (WIFEXITED(status)) {
	if ((signum = WEXITSTATUS(status))) {
	    if (cp)
		fprintf (fp, "%s: ", cp);
	    fprintf (fp, "exit %d\n", signum);
	}
	return signum;
    }

    if (WIFSIGNALED(status)) {
	/* If child process terminated due to receipt of a signal */
	signum = WTERMSIG(status);
	if (signum != SIGINT) {
	    if (cp)
		fprintf (fp, "%s: ", cp);
	    fprintf (fp, "signal %d", signum);
	    errno = 0;
	    signame = strsignal(signum);
	    if (errno)
		signame = NULL;
	    if (signame)
		fprintf (fp, " (%s%s)\n", signame,
			 WCOREDUMP(status) ? ", core dumped" : "");
	    else {
                if (WCOREDUMP(status))
                    fputs(" (core dumped)", fp);
                putc('\n', fp);
            }
	}
    }

    return status;
}
