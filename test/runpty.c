/* runpty.c - Run a process under a pseudo-tty
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <time.h>
#include <errno.h>

#define COMMAND_TIMEOUT 30

static void run_command(char *argv[], int master_in, int master_out);
static int open_master_pty(const char *desc);
static void die(const char *fmt, ...);

int
main(int argc, char *argv[])
{
    int master_in, master_out, cc, status;
    time_t starttime, now;
    pid_t child;
    unsigned char readbuf[1024];
    FILE *output;

    if (argc < 3) {
        die("%s: too few arguments\n"
            "usage: %s output-filename command [arguments...]\n",
            *argv, *argv);
    }

    master_in = open_master_pty("input");
    master_out = open_master_pty("output");

    if ((child = fork()) == -1) {
	die("fork() failed: %s\n", strerror(errno));
    }
    if (child == 0) {
        run_command(argv + 2, master_in, master_out); /* Does not return. */
    }

    if (!(output = fopen(argv[1], "w"))) {
	die("Unable to open \"%s\" for output: %s\n", argv[1],
            strerror(errno));
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
	    die("select() failed: %s\n", strerror(errno));
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

        now = time(NULL);
	if (now >= starttime + COMMAND_TIMEOUT) {
	    fprintf(stderr, "Command execution timed out: %ld to %ld: %d\n",
                starttime, now, cc);
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

static void
run_command(char *argv[], int master_in, int master_out)
{
    const char *slavename;
    int slave;

    /* Open the two slave pseudo-ttys and close the masters after we are
     * done with them. */

    if (!(slavename = ptsname(master_in))) {
        die("Unable to determine name of slave pty: %s\n",
            strerror(errno));
    }

    if ((slave = open(slavename, O_RDWR)) == -1) {
        die("Unable to open slave pty \"%s\": %s\n", slavename,
            strerror(errno));
    }

    dup2(slave, STDIN_FILENO);
    close(slave);
    close(master_in);

    if (!(slavename = ptsname(master_out))) {
        die("Unable to determine name of slave pty: %s\n",
            strerror(errno));
    }

    if ((slave = open(slavename, O_RDWR | O_NOCTTY)) < 0) {
        die("Unable to open slave pty \"%s\": %s\n", slavename,
            strerror(errno));
    }

    dup2(slave, STDOUT_FILENO);
    dup2(slave, STDERR_FILENO);
    close(slave);
    close(master_out);

    execvp(*argv, argv);

    die("execvp(%s) failed: %s\n", *argv, strerror(errno));
}

static int
open_master_pty(const char *desc)
{
    int fd;

    if ((fd = posix_openpt(O_RDWR | O_NOCTTY)) == -1) {
	die("Unable to open master %s pseudo-tty: %s\n", desc, strerror(errno));
    }
    if (grantpt(fd) == -1) {
	die("Unable to grant permissions to master %s pty: %s\n", desc,
            strerror(errno));
    }
    if (unlockpt(fd) == -1) {
	die("Unable to unlock master %s pty: %s\n", desc, strerror(errno));
    }

    return fd;
}

static void
die(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    exit(1);
}
