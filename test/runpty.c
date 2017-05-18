/*
 * runpty.c - Run a process under a pseudo-tty
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <time.h>
#include <errno.h>

#define COMMAND_TIMEOUT 30

int
main(int argc, char *argv[])
{
    int master, slave, cc, sendeof = 0, status;
    time_t starttime;
    const char *slavename;
    pid_t child;
    unsigned char readbuf[1024];
    FILE *output;

    if (argc < 3) {
	fprintf(stderr, "Usage: %s output-filename command [arguments ...]\n",
		argv[0]);
	exit(1);
    }

    if ((master = posix_openpt(O_RDWR | O_NOCTTY)) < 0) {
	fprintf(stderr, "Unable to open master pseudo-tty: %s\n",
		strerror(errno));
	exit(1);
    }

    if (grantpt(master) < 0) {
	fprintf(stderr, "Unable to grant permissions to master pty: %s\n",
		strerror(errno));
	exit(1);
    }

    if (unlockpt(master) < 0) {
	fprintf(stderr, "Unable to unlock master pty: %s\n", strerror(errno));
	exit(1);
    }

    if (!(slavename = ptsname(master))) {
	fprintf(stderr, "Unable to determine name of slave pty: %s\n",
		strerror(errno));
	exit(1);
    }

    if ((slave = open(slavename, O_RDWR | O_NOCTTY)) < 0) {
	fprintf(stderr, "Unable to open slave pty \"%s\": %s\n", slavename,
		strerror(errno));
	exit(1);
    }

    child = fork();

    /*
     * Start the child process if we are in the child; close the master
     * as we are supposed to only use the slave ptys here.
     */

    if (child == 0) {
	close(master);
	dup2(slave, STDIN_FILENO);
	dup2(slave, STDOUT_FILENO);
	dup2(slave, STDERR_FILENO);
	close(slave);

	execvp(argv[2], argv + 2);

	fprintf(stderr, "execvp(%s) failed: %s\n", argv[2], strerror(errno));
    } else if (child < 0) {
	fprintf(stderr, "fork() failed: %s\n", strerror(errno));
	exit(1);
    }

    close(slave);

    if (!(output = fopen(argv[1], "w"))) {
	fprintf(stderr, "Unable to open \"%s\" for output: %s\n", argv[1],
		strerror(errno));
	exit(1);
    }

    starttime = time(NULL);

    for (;;) {
    	fd_set readfds;
	struct timeval tv;

	FD_ZERO(&readfds);

	FD_SET(master, &readfds);

	/*
	 * Okay, what's going on here?
	 *
	 * We want to send the EOF character (to simulate a "end of file",
	 * as if we redirected stdin to /dev/null).  We can't just close
	 * master, because we won't be able to get any data back.  So,
	 * after we get SOME data, set sendeof, and that will cause us to
	 * send the EOF character after the first select call.  If we are
	 * doing sendeof, set the timeout to 1 second; otherwise, set the
	 * timeout to COMMAND_TIMEOUT seconds remaining.
	 */

	if (sendeof) {
	    tv.tv_sec = 1;
	} else {
	    tv.tv_sec = starttime + COMMAND_TIMEOUT - time(NULL);
	    if (tv.tv_sec < 0)
		tv.tv_sec = 0;
	}
	tv.tv_usec = 0;

	cc = select(master + 1, &readfds, NULL, NULL, &tv);

	if (cc < 0) {
	    fprintf(stderr, "select() failed: %s\n", strerror(errno));
	    exit(1);
	}

	if (cc > 0 && FD_ISSET(master, &readfds)) {
	    cc = read(master, readbuf, sizeof(readbuf));

	    if (cc <= 0)
		break;

	    fwrite(readbuf, 1, cc, output);
	}

	if (cc == 0 && sendeof) {
	    struct termios rtty;

	    if (tcgetattr(master, &rtty) < 0) {
		fprintf(stderr, "tcgetattr() failed: %s\n", strerror(errno));
		exit(1);
	    }

	    if (rtty.c_lflag & ICANON)
		write(master, &rtty.c_cc[VEOF], 1);
	} else if (!sendeof)
	    sendeof = 1;

	if (time(NULL) > starttime + COMMAND_TIMEOUT) {
	    fprintf(stderr, "Command execution timed out\n");
	    break;
	}
    }

    close(master);
    fclose(output);

    waitpid(child, &status, 0);

    exit(0);
}
