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
#include <errno.h>
#include <sys/types.h>
#include <limits.h>

#define PIDFILE "/tmp/fakepop.pid"
#define LINESIZE 1024
#define BUFALLOC 4096

#define CHECKUSER()	if (!user) { \
				putcrlf(s, "-ERR Aren't you forgetting " \
				       "something?  Like the USER command?"); \
				continue; \
			}
#define CHECKAUTH()	if (!auth) { \
				putcrlf(s, "-ERR Um, hello?  Forget to " \
				       "log in?"); \
				continue; \
			}

void putcrlf(int, char *);
int serve(const char *, const char *);

static void putpopbulk(int, char *);
static int getpop(int, char *, ssize_t);
static char *readmessage(FILE *);

int
main(int argc, char *argv[])
{
	FILE **mfiles;
	char line[LINESIZE];
	int rc, s, user = 0, auth = 0, i, j;
	int numfiles;
	size_t *octets;
	const char *xoauth;

	if (argc < 5) {
		fprintf(stderr, "Usage: %s port username "
			"password mail-file [mail-file ...]\n", argv[0]);
		exit(1);
	}

	if (strcmp(argv[2], "XOAUTH") == 0) {
		xoauth = argv[3];
	} else {
		xoauth = NULL;
	}

	numfiles = argc - 4;

	mfiles = malloc(sizeof(FILE *) * numfiles);

	if (! mfiles) {
		fprintf(stderr, "Unable to allocate %d bytes for file "
			"array\n", (int) (sizeof(FILE *) * numfiles));
		exit(1);
	}

	octets = malloc(sizeof(*octets) * numfiles);

	if (! octets) {
		fprintf(stderr, "Unable to allocate %d bytes for size "
			"array\n", (int) (sizeof(FILE *) * numfiles));
		exit(1);
	}

	for (i = 4, j = 0; i < argc; i++, j++) {
		if (!(mfiles[j] = fopen(argv[i], "r"))) {
			fprintf(stderr, "Unable to open message file \"%s\""
				": %s\n", argv[i], strerror(errno));
			exit(1);
		}

		/*
		 * POP wants the size of the maildrop in bytes, but
		 * with \r\n line endings.  Calculate that.
		 */

		octets[j] = 0;

		while (fgets(line, sizeof(line), mfiles[j])) {
			octets[j] += strlen(line);
			if (strrchr(line, '\n'))
				octets[j]++;
		}

		rewind(mfiles[j]);
	}

	s = serve(PIDFILE, argv[1]);

	/*
	 * Pretend to be a POP server
	 */

	putcrlf(s, "+OK Not really a POP server, but we play one on TV");

	for (;;) {
		char linebuf[LINESIZE];

		rc = getpop(s, linebuf, sizeof(linebuf));

		if (rc <= 0)
			break;	/* Error or EOF */

		if (strcasecmp(linebuf, "CAPA") == 0) {
			putpopbulk(s, "+OK We have no capabilities, really\r\n"
				   "FAKE-CAPABILITY\r\n");
			if (xoauth != NULL) {
				putcrlf(s, "SASL XOAUTH2");
			}
			putcrlf(s, ".");
		} else if (strncasecmp(linebuf, "USER ", 5) == 0) {
			if (strcmp(linebuf + 5, argv[2]) == 0) {
				putcrlf(s, "+OK Niiiice!");
				user = 1;
			} else {
				putcrlf(s, "-ERR Don't play me, bro!");
			}
		} else if (strncasecmp(linebuf, "PASS ", 5) == 0) {
			CHECKUSER();
			if (strcmp(linebuf + 5, argv[3]) == 0) {
				putcrlf(s, "+OK Aren't you a sight "
				       "for sore eyes!");
				auth = 1;
			} else {
				putcrlf(s, "-ERR C'mon!");
			}
		} else if (xoauth != NULL
			   && strncasecmp(linebuf, "AUTH XOAUTH2", 12) == 0) {
			if (strstr(linebuf, xoauth) == NULL) {
				putcrlf(s, "+ base64-json-err");
				rc = getpop(s, linebuf, sizeof(linebuf));
				if (rc != 0)
					break;	/* Error or EOF */
				putcrlf(s, "-ERR [AUTH] Invalid credentials.");
				continue;
			}
			putcrlf(s, "+OK Welcome.");
			auth = 1;
		} else if (strcasecmp(linebuf, "STAT") == 0) {
			size_t total = 0;
			CHECKAUTH();
			for (i = 0, j = 0; i < numfiles; i++) {
				if (mfiles[i]) {
					total += octets[i];
					j++;
				}
			}
			snprintf(linebuf, sizeof(linebuf),
					 "+OK %d %d", i, (int) total);
			putcrlf(s, linebuf);
		} else if (strncasecmp(linebuf, "RETR ", 5) == 0) {
			CHECKAUTH();
			rc = sscanf(linebuf + 5, "%d", &i);
			if (rc != 1) {
				putcrlf(s, "-ERR Whaaaa...?");
				continue;
			}
			if (i < 1 || i > numfiles) {
				putcrlf(s, "-ERR That message number is "
				       "out of range, jerkface!");
				continue;
			}
			if (mfiles[i - 1] == NULL) {
				putcrlf(s, "-ERR Sorry, don't have it anymore");
			} else {
				char *buf = readmessage(mfiles[i - 1]);
				putcrlf(s, "+OK Here you go ...");
				putpopbulk(s, buf);
				free(buf);
			}
		} else if (strncasecmp(linebuf, "DELE ", 5) == 0) {
			CHECKAUTH();
			rc = sscanf(linebuf + 5, "%d", &i);
			if (rc != 1) {
				putcrlf(s, "-ERR Whaaaa...?");
				continue;
			}
			if (i < 1 || i > numfiles) {
				putcrlf(s, "-ERR That message number is "
				       "out of range, jerkface!");
				continue;
			}
			if (mfiles[i - 1] == NULL) {
				putcrlf(s, "-ERR Um, didn't you tell me "
				       "to delete it already?");
			} else {
				fclose(mfiles[i - 1]);
				mfiles[i - 1] = NULL;
				putcrlf(s, "+OK Alright man, I got rid of it");
			}
		} else if (strcasecmp(linebuf, "QUIT") == 0) {
			putcrlf(s, "+OK See ya, wouldn't want to be ya!");
			close(s);
			break;
		} else {
			putcrlf(s, "-ERR Um, what?");
		}
	}

	exit(0);
}

/*
 * Put one big buffer to the POP server.  Should have already had the line
 * endings set up and dot-stuffed if necessary.
 */

static void
putpopbulk(int socket, char *data)
{
	ssize_t datalen = strlen(data);

	if (write(socket, data, datalen) < 0) {
	    perror ("write");
	}
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

	rewind(file);

	return buffer;
}
