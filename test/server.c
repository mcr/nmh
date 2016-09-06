/*
 * server.c - Utilities for fake servers used by the nmh test suite
 *
 * This code is Copyright (c) 2014, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <signal.h>

static const char *PIDFN = NULL;

static void killpidfile(void);
static void handleterm(int);

#ifndef EPROTOTYPE
#define EPROTOTYPE 0
#endif

static int
try_bind(int socket, const struct sockaddr *address, socklen_t len)
{
	int i, status;
	for (i = 0; i < 5; i++) {
		if ((status = bind(socket, address, len)) == 0) {
			return 0;
		}
		sleep(1);
	}

	return status;
}

int
serve(const char *pidfn, const char *port)
{
	struct addrinfo hints, *res;
	int rc, l, conn, on;
	FILE *pid;
	pid_t child;
	fd_set readfd;
	struct stat st;
	struct timeval tv;

	PIDFN = pidfn;

	/*
	 * If there is a pid file already around, kill the previously running
	 * fakesmtp process.  Hopefully this will reduce the race conditions
	 * that crop up when running the test suite.
	 */

	if (stat(pidfn, &st) == 0) {
		long oldpid;

		if (!(pid = fopen(pidfn, "r"))) {
			fprintf(stderr, "Cannot open %s (%s), continuing ...\n",
				pidfn, strerror(errno));
		} else {
			rc = fscanf(pid, "%ld", &oldpid);
			fclose(pid);

			if (rc != 1) {
				fprintf(stderr, "Unable to parse pid in %s,"
					" continuing ...\n",
					pidfn);
			} else {
				kill((pid_t) oldpid, SIGTERM);
			}
		}

		unlink(pidfn);
	}

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	rc = getaddrinfo("127.0.0.1", port, &hints, &res);

	if (rc) {
		fprintf(stderr, "Unable to resolve localhost/%s: %s\n",
			port, gai_strerror(rc));
		exit(1);
	}

	l = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	if (l == -1) {
		fprintf(stderr, "Unable to create listening socket: %s\n",
			strerror(errno));
		exit(1);
	}

	on = 1;

	if (setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		fprintf(stderr, "Unable to set SO_REUSEADDR: %s\n",
			strerror(errno));
		exit(1);
	}

	if (try_bind(l, res->ai_addr, res->ai_addrlen) == -1) {
		fprintf(stderr, "Unable to bind socket: %s\n", strerror(errno));
		exit(1);
	}

	freeaddrinfo(res);

	if (listen(l, 1) == -1) {
		fprintf(stderr, "Unable to listen on socket: %s\n",
			strerror(errno));
		exit(1);
	}

	/*
	 * Now we fork() and print out the process ID of our child
	 * for scripts to use.  Once we do that, then exit.
	 */

	child = fork();

	switch (child) {
	case -1:
		fprintf(stderr, "Unable to fork child: %s\n", strerror(errno));
		exit(1);
		break;
	case 0:
		/*
		 * Close stdin & stdout, otherwise people can
		 * think we're still doing stuff.  For now leave stderr
		 * open.
		 */
		fclose(stdin);
		fclose(stdout);
		break;
	default:
		/* XXX why?  it's never used... */
		printf("%ld\n", (long) child);
		exit(0);
	}

	/*
	 * Now that our socket & files are set up, wait 30 seconds for
	 * a connection.  If there isn't one, then exit.
	 */

	if (!(pid = fopen(pidfn, "w"))) {
		fprintf(stderr, "Cannot open %s: %s\n",
			pidfn, strerror(errno));
		exit(1);
	}

	fprintf(pid, "%ld\n", (long) getpid());
	fclose(pid);

	signal(SIGTERM, handleterm);
	atexit(killpidfile);

	FD_ZERO(&readfd);
	FD_SET(l, &readfd);
	tv.tv_sec = 30;
	tv.tv_usec = 0;

	rc = select(l + 1, &readfd, NULL, NULL, &tv);

	if (rc < 0) {
		fprintf(stderr, "select() failed: %s\n", strerror(errno));
		exit(1);
	}

	/*
	 * I think if we get a timeout, we should just exit quietly.
	 */

	if (rc == 0) {
		exit(1);
	}

	/*
	 * Alright, got a connection!  Accept it.
	 */

	if ((conn = accept(l, NULL, NULL)) == -1) {
		fprintf(stderr, "Unable to accept connection: %s\n",
			strerror(errno));
		exit(1);
	}

	close(l);

    return conn;
}

/*
 * Write a line (adding \r\n) to the client on the other end
 */
void
putcrlf(int socket, char *data)
{
	struct iovec iov[2];

	iov[0].iov_base = data;
	iov[0].iov_len = strlen(data);
	iov[1].iov_base = "\r\n";
	iov[1].iov_len = 2;

	/* ECONNRESET just means the client already closed its end */
	/* MacOS X can also return EPROTOTYPE (!) here sometimes */
	/* XXX is it useful to log errors here at all? */
	if (writev(socket, iov, 2) < 0 && errno != ECONNRESET &&
	    errno != EPROTOTYPE) {
	    perror ("server writev");
	}
}

/*
 * Handle a SIGTERM
 */

static void
handleterm(int signal)
{
	(void) signal;

	killpidfile();
	fflush(NULL);
	_exit(1);
}

/*
 * Get rid of our pid file
 */

static void
killpidfile(void)
{
	if (PIDFN != NULL) {
		unlink(PIDFN);
	}
}
