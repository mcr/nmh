/*
 * fakesmtp - A fake SMTP server used by the nmh test suite
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
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
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <signal.h>

#define PIDFILE "/tmp/fakesmtp.pid"

#define LINESIZE 1024

static void killpidfile(void);
static void handleterm(int);
static void putsmtp(int, char *);
static int getsmtp(int, char *);

int
main(int argc, char *argv[])
{
	struct addrinfo hints, *res;
	int rc, l, conn, on, datamode;
	FILE *f, *pid;
	fd_set readfd;
	struct stat st;
	struct timeval tv;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s output-filename port\n", argv[0]);
		exit(1);
	}

	if (!(f = fopen(argv[1], "w"))) {
		fprintf(stderr, "Unable to open output file \"%s\": %s\n",
			argv[1], strerror(errno));
		exit(1);
	}

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	rc = getaddrinfo("127.0.0.1", argv[2], &hints, &res);

	if (rc) {
		fprintf(stderr, "Unable to resolve localhost/%s: %s\n",
			argv[2], gai_strerror(rc));
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

	if (bind(l, res->ai_addr, res->ai_addrlen) == -1) {
		fprintf(stderr, "Unable to bind socket: %s\n", strerror(errno));
		exit(1);
	}

	if (listen(l, 1) == -1) {
		fprintf(stderr, "Unable to listen on socket: %s\n",
			strerror(errno));
		exit(1);
	}

	/*
	 * Now that our socket & files are set up, do the following things:
	 *
	 * - Check for a PID file.  If there is one, kill the old version.
	 * - Wait 30 seconds for a connection.  If there isn't one, then
	 *   exit.
	 */

	if (stat(PIDFILE, &st) == 0) {
		long oldpid;

		if (!(pid = fopen(PIDFILE, "r"))) {
			fprintf(stderr, "Cannot open " PIDFILE
				" (%s), continuing ...\n", strerror(errno));
		} else {
			rc = fscanf(pid, "%ld", &oldpid);
			fclose(pid);

			if (rc != 1) {
				fprintf(stderr, "Unable to parse pid in "
					PIDFILE ", continuing ...\n");
			} else {
				kill((pid_t) oldpid, SIGTERM);
			}
		}

		unlink(PIDFILE);
	}

	if (!(pid = fopen(PIDFILE, "w"))) {
		fprintf(stderr, "Cannot open " PIDFILE ": %s\n",
			strerror(errno));
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
	
	/*
	 * Pretend to be an SMTP server.
	 */

	putsmtp(conn, "220 Not really an ESMTP server");
	datamode = 0;

	for (;;) {
		char line[LINESIZE];

		rc = getsmtp(conn, line);

		if (rc == -1)
			break;	/* EOF */

		fprintf(f, "%s\n", line);

		/*
		 * If we're in DATA mode, then check to see if we've got
		 * a "."; otherwise, continue
		 */

		if (datamode) {
			if (strcmp(line, ".") == 0) {
				datamode = 0;
				putsmtp(conn, "250 Thanks for the info!");
			}
			continue;
		}

		/*
		 * Most commands we ignore and send the same response to.
		 */

		if (strcmp(line, "QUIT") == 0) {
			putsmtp(conn, "221 Later alligator!");
			close(conn);
			break;
		} else if (strcmp(line, "DATA") == 0) {
			putsmtp(conn, "354 Go ahead");
			datamode = 1;
		} else {
			putsmtp(conn, "250 I'll buy that for a dollar!");
		}
	}

	fclose(f);

	exit(0);
}

/*
 * Write a line to the SMTP client on the other end
 */

static void
putsmtp(int socket, char *data)
{
	struct iovec iov[2];

	iov[0].iov_base = data;
	iov[0].iov_len = strlen(data);
	iov[1].iov_base = "\r\n";
	iov[1].iov_len = 2;

	writev(socket, iov, 2);
}

/*
 * Read a line (up to the \r\n)
 */

static int
getsmtp(int socket, char *data)
{
	int cc;
	static unsigned int bytesinbuf = 0;
	static char buffer[LINESIZE * 2], *p;

	for (;;) {
		/*
		 * Find our \r\n
		 */

		if (bytesinbuf > 0 && (p = strchr(buffer, '\r')) &&
							*(p + 1) == '\n') {
			*p = '\0';
			strncpy(data, buffer, LINESIZE);
			data[LINESIZE - 1] = '\0';
			cc = strlen(buffer);

			/*
			 * Shuffle leftover bytes back to the beginning
			 */

			bytesinbuf -= cc + 2;	/* Don't forget \r\n */
			if (bytesinbuf > 0) {
				memmove(buffer, buffer + cc + 2, bytesinbuf);
			}
			return cc;
		}

		if (bytesinbuf >= sizeof(buffer)) {
			fprintf(stderr, "Buffer overflow in getsmtp()!\n");
			exit(1);
		}

		memset(buffer + bytesinbuf, 0, sizeof(buffer) - bytesinbuf);
		cc = read(socket, buffer + bytesinbuf,
			  sizeof(buffer) - bytesinbuf);

		if (cc < 0) {
			fprintf(stderr, "Read failed: %s\n", strerror(errno));
			exit(1);
		}

		if (cc == 0)
			return -1;

		bytesinbuf += cc;
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
	unlink(PIDFILE);
}
