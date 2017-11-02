/* pidstatus.c -- report child's status
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"

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
    char *mesg;
    int num;
    bool lookup;
    char *signame;

    if (WIFEXITED(status)) {
	status = WEXITSTATUS(status);
	if (status) {
	    if (cp)
		fprintf (fp, "%s: ", cp);
	    fprintf(fp, "exited %d\n", status);
	}
	return status;
    }

    if (WIFSIGNALED(status)) {
        mesg = "signalled";
        num = WTERMSIG(status);
	if (num == SIGINT)
            return status;
        lookup = true;
    } else if (WIFSTOPPED(status)) {
        mesg = "stopped";
        num = WSTOPSIG(status);
        lookup = true;
    } else if (WIFCONTINUED(status)) {
        mesg = "continued";
        num = -1;
        lookup = false;
    } else {
        mesg = "bizarre wait(2) status";
        num = status;
        lookup = false;
    }

    if (cp)
        fprintf(fp, "%s: ", cp);
    fputs(mesg, fp);

    if (num != -1) {
        fprintf(fp, " %#x", num);
        if (lookup) {
            errno = 0;
            signame = strsignal(num);
            if (errno)
                signame = "invalid";
            putc(' ', fp);
            fputs(signame, fp);
        }
    }
    if (WCOREDUMP(status))
        fputs(", core dumped", fp);
    putc('\n', fp);

    return status;
}
