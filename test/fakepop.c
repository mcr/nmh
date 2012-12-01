/*
 * fakepop - A fake POP server used by the nmh test suite
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
#include <limits.h>
#include <signal.h>

#define PIDFILE "/tmp/fakepop.pid"
#define LINESIZE 1024
#define BUFALLOC 4096

#define CHECKUSER()	if (!user) { \
				putpop(s, "-ERR Aren't you forgetting " \
				       "something?  Like the USER command?"); \
				continue; \
			}
#define CHECKUSERPASS()	CHECKUSER() \
			if (! pass) { \
				putpop(s, "-ERR Um, hello?  Forget to " \
				       "log in?"); \
				continue; \
			}

static void killpidfile(void);
static void handleterm(int);
static void putpop(int, char *);
static void putpopbulk(int, char *);
static int getpop(int, char *, ssize_t);
static char *readmessage(FILE *);

int
main(int argc, char *argv[])
{
	struct addrinfo hints, *res;
	struct stat st;
	FILE *f, *pid;
	char line[LINESIZE];
	fd_set readfd;
	struct timeval tv;
	pid_t child;
	int octets = 0, rc, l, s, on, user = 0, pass = 0, deleted = 0;

	if (argc != 5) {
		fprintf(stderr, "Usage: %s mail-file port username "
			"password\n", argv[0]);
		exit(1);
	}

	if (!(f = fopen(argv[1], "r"))) {
		fprintf(stderr, "Unable to open message file \"%s\": %s\n",
			argv[1], strerror(errno));
		exit(1);
	}

	/*
	 * POP wants the size of the maildrop in bytes, but with \r\n line
	 * endings.  Calculate that.
	 */

	while (fgets(line, sizeof(line), f)) {
		octets += strlen(line);
		if (strrchr(line, '\n'))
			octets++;
	}

	rewind(f);

	/*
	 * If there is a pid file around, kill the previously running
	 * fakepop process.
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
	 * Fork off a copy of ourselves, print out our child pid, then
	 * exit.
	 */

	switch (child = fork()) {
	case -1:
		fprintf(stderr, "Unable to fork child: %s\n", strerror(errno));
		exit(1);
		break;
	case 0:
		/*
		 * Close stdin and stdout so $() in the shell will get an
		 * EOF.  For now leave stderr open.
		 */
		fclose(stdin);
		fclose(stdout);
		break;
	default:
		printf("%ld\n", (long) child);
		exit(0);
	}

	/*
	 * Now that our socket and files are set up, wait 30 seconds for
	 * a connection.  If there isn't one, then exit.
	 */

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
	 * If we get a timeout, just silently exit
	 */

	if (rc == 0) {
		exit(1);
	}

	/*
	 * We got a connection; accept it.  Right after that close our
	 * listening socket so we won't get any more connections on it.
	 */

	if ((s = accept(l, NULL, NULL)) == -1) {
		fprintf(stderr, "Unable to accept connection: %s\n",
			strerror(errno));
		exit(1);
	}

	close(l);

	/*
	 * Pretend to be a POP server
	 */

	putpop(s, "+OK Not really a POP server, but we play one on TV");

	for (;;) {
		char linebuf[LINESIZE];

		rc = getpop(s, linebuf, sizeof(linebuf));

		if (rc <= 0)
			break;	/* Error or EOF */

		if (strcasecmp(linebuf, "CAPA") == 0) {
			putpopbulk(s, "+OK We have no capabilities, really\r\n"
				   "FAKE-CAPABILITY\r\n.\r\n");
		} else if (strncasecmp(linebuf, "USER ", 5) == 0) {
			if (strcmp(linebuf + 5, argv[3]) == 0) {
				putpop(s, "+OK Niiiice!");
				user = 1;
			} else {
				putpop(s, "-ERR Don't play me, bro!");
			}
		} else if (strncasecmp(linebuf, "PASS ", 5) == 0) {
			CHECKUSER();
			if (strcmp(linebuf + 5, argv[4]) == 0) {
				putpop(s, "+OK Aren't you a sight "
				       "for sore eyes!");
				pass = 1;
			} else {
				putpop(s, "-ERR C'mon!");
			}
		} else if (strcasecmp(linebuf, "STAT") == 0) {
			CHECKUSERPASS();
			if (deleted) {
				strncpy(linebuf, "+OK 0 0", sizeof(linebuf));
			} else {
				snprintf(linebuf, sizeof(linebuf),
					 "+OK 1 %d", octets);
			}
			putpop(s, linebuf);
		} else if (strcasecmp(linebuf, "RETR 1") == 0) {
			CHECKUSERPASS();
			if (deleted) {
				putpop(s, "-ERR Sorry, don't have it anymore");
			} else {
				char *buf = readmessage(f);
				putpop(s, "+OK Here you go ...");
				putpopbulk(s, buf);
				free(buf);
			}
		} else if (strncasecmp(linebuf, "RETR ", 5) == 0) {
			CHECKUSERPASS();
			putpop(s, "-ERR Sorry man, out of range!");
		} else if (strcasecmp(linebuf, "DELE 1") == 0) {
			CHECKUSERPASS();
			if (deleted) {
				putpop(s, "-ERR Um, didn't you tell me "
				       "to delete it already?");
			} else {
				putpop(s, "+OK Alright man, I got rid of it");
				deleted = 1;
			}
		} else if (strncasecmp(linebuf, "DELE ", 5) == 0) {
			CHECKUSERPASS();
			putpop(s, "-ERR Sorry man, out of range!");
		} else if (strcasecmp(linebuf, "QUIT") == 0) {
			putpop(s, "+OK See ya, wouldn't want to be ya!");
			close(s);
			break;
		} else {
			putpop(s, "-ERR Um, what?");
		}
	}

	exit(0);
}

/*
 * Send one line to the POP client
 */

static void
putpop(int socket, char *data)
{
	struct iovec iov[2];

	iov[0].iov_base = data;
	iov[0].iov_len = strlen(data);
	iov[1].iov_base = "\r\n";
	iov[1].iov_len = 2;

	writev(socket, iov, 2);
}

/*
 * Put one big buffer to the POP server.  Should have already had the line
 * endings set up and dot-stuffed if necessary.
 */

static void
putpopbulk(int socket, char *data)
{
	ssize_t datalen = strlen(data);

	write(socket, data, datalen);
}

/*
 * Get one line from the POP server.  We don't do any buffering here.
 */

static int
getpop(int socket, char *data, ssize_t len)
{
	int cc;
	int offset = 0;

	for (;;) {
		cc = read(socket, data + offset, len - offset);

		if (cc < 0) {
			fprintf(stderr, "Read failed: %s\n", strerror(errno));
			exit(1);
		}

		if (cc == 0) {
			return 0;
		}

		offset += cc;

		if (offset >= len) {
			fprintf(stderr, "Input buffer overflow "
				"(%d bytes)\n", (int) len);
			exit(1);
		}

		if (data[offset - 1] == '\n' && data[offset - 2] == '\r') {
			data[offset - 2] = '\0';
			return offset - 2;
		}
	}
}

#define HAVEROOM(buf, size, used, new) do { \
		if (used + new > size - 1) { \
			buf = realloc(buf, size += BUFALLOC); \
		} \
	} while (0)
	
/*
 * Read a file and return it as one malloc()'d buffer.  Convert \n to \r\n
 * and dot-stuff if necessary.
 */

static char *
readmessage(FILE *file)
{
	char *buffer = malloc(BUFALLOC);
	ssize_t bufsize = BUFALLOC, used = 0;
	char linebuf[LINESIZE];
	int i;

	buffer[0] = '\0';

	while (fgets(linebuf, sizeof(linebuf), file)) {
		if (strcmp(linebuf, ".\n") == 0) {
			HAVEROOM(buffer, bufsize, used, 4);
			strcat(buffer, "..\r\n");
		} else {
			i = strlen(linebuf);
			if (i && linebuf[i - 1] == '\n') {
				HAVEROOM(buffer, bufsize, used, i + 1);
				linebuf[i - 1] = '\0';
				strcat(buffer, linebuf);
				strcat(buffer, "\r\n");
			} else {
				HAVEROOM(buffer, bufsize, used, i);
				strcat(buffer, linebuf);
			}
		}
	}

	/*
	 * Put a terminating dot at the end
	 */

	HAVEROOM(buffer, bufsize, used, 3);

	strcat(buffer, ".\r\n");

	return buffer;
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
