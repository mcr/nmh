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
    int master_in, master_out, slave, cc, status;
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

    if ((master_in = posix_openpt(O_RDWR | O_NOCTTY)) < 0) {
	fprintf(stderr, "Unable to open master pseudo-tty: %s\n",
		strerror(errno));
	exit(1);
    }

    if ((master_out = posix_openpt(O_RDWR | O_NOCTTY)) < 0) {
	fprintf(stderr, "Unable to open master pseudo-tty: %s\n",
		strerror(errno));
	exit(1);
    }

    if (grantpt(master_in) < 0) {
	fprintf(stderr, "Unable to grant permissions to master pty: %s\n",
		strerror(errno));
	exit(1);
    }

    if (grantpt(master_out) < 0) {
	fprintf(stderr, "Unable to grant permissions to master pty: %s\n",
		strerror(errno));
	exit(1);
    }

    if (unlockpt(master_in) < 0) {
	fprintf(stderr, "Unable to unlock master pty: %s\n", strerror(errno));
	exit(1);
    }

    if (unlockpt(master_out) < 0) {
	fprintf(stderr, "Unable to unlock master pty: %s\n", strerror(errno));
	exit(1);
    }

    child = fork();

    /*
     * Start the child process if we are in the child; open the two
     * slave pseudo-ttys and close the masters after we are done with them.
     */

    if (child == 0) {
	if (!(slavename = ptsname(master_in))) {
	    fprintf(stderr, "Unable to determine name of slave pty: %s\n",
		    strerror(errno));
	    exit(1);
	}

	if ((slave = open(slavename, O_RDWR)) < 0) {
	    fprintf(stderr, "Unable to open slave pty \"%s\": %s\n", slavename,
		    strerror(errno));
	    exit(1);
	}

	dup2(slave, STDIN_FILENO);
	close(slave);
	close(master_in);

	if (!(slavename = ptsname(master_out))) {
	    fprintf(stderr, "Unable to determine name of slave pty: %s\n",
		    strerror(errno));
	    exit(1);
	}

	if ((slave = open(slavename, O_RDWR | O_NOCTTY)) < 0) {
	    fprintf(stderr, "Unable to open slave pty \"%s\": %s\n", slavename,
		    strerror(errno));
	    exit(1);
	}

	dup2(slave, STDOUT_FILENO);
	dup2(slave, STDERR_FILENO);
	close(slave);
	close(master_out);

	execvp(argv[2], argv + 2);

	fprintf(stderr, "execvp(%s) failed: %s\n", argv[2], strerror(errno));
	exit(1);

    } else if (child < 0) {
	fprintf(stderr, "fork() failed: %s\n", strerror(errno));
	exit(1);
    }

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

	FD_SET(master_out, &readfds);

	/*
	 * After we get our first bit of data, close the master pty
	 * connected to standard input on our slave; that will generate
	 * an EOF.
	 */

	tv.tv_sec = starttime + COMMAND_TIMEOUT - time(NULL);
	if (tv.tv_sec < 0)
	    tv.tv_sec = 0;
	tv.tv_usec = 0;

	cc = select(master_out + 1, &readfds, NULL, NULL, &tv);

	if (cc < 0) {
	    fprintf(stderr, "select() failed: %s\n", strerror(errno));
	    exit(1);
	}

	if (cc > 0 && FD_ISSET(master_out, &readfds)) {
	    cc = read(master_out, readbuf, sizeof(readbuf));

	    if (cc <= 0)
		break;

	    fwrite(readbuf, 1, cc, output);

	    if (master_in != -1) {
		close(master_in);
		master_in = -1;
	    }
	}

	if (time(NULL) >= starttime + COMMAND_TIMEOUT) {
	    fprintf(stderr, "Command execution timed out\n");
	    break;
	}
    }

    if (master_in != -1)
	close(master_in);
    close(master_out);
    fclose(output);

    waitpid(child, &status, 0);

    exit(0);
}
