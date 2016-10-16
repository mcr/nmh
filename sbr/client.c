
/*
 * client.c -- connect to a server
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/mts.h>
#include <h/utils.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define	MAXARGS   1000

/*
 * static prototypes
 */

/* client's own static version of several nmh subroutines */
static char **client_brkstring (char *, char *, char *);
static int client_brkany (char, char *);
static char **client_copyip (char **, char **, int);
static void client_freelist(char **);


int
client (char *args, char *service, char *response, int len_response, int debug)
{
    int sd, rc;
    char **ap, *arguments[MAXARGS];
    struct addrinfo hints, *res, *ai;

    ap = arguments;
    if (args != NULL && *args != 0) {
	ap = client_copyip (client_brkstring (getcpy(args), " ", "\n"),
		ap, MAXARGS);
    } else {
	if (servers != NULL && *servers != 0)
	    ap = client_copyip (client_brkstring (getcpy(servers), " ", "\n"),
		ap, MAXARGS);
    }
    if (ap == arguments) {
	*ap++ = mh_xstrdup("localhost");
	*ap = NULL;
    }

    memset(&hints, 0, sizeof(hints));
#ifdef AI_ADDRCONFIG
    hints.ai_flags = AI_ADDRCONFIG;
#endif
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    for (ap = arguments; *ap; ap++) {

	if (debug) {
	    fprintf(stderr, "Trying to connect to \"%s\" ...\n", *ap);
	}

	rc = getaddrinfo(*ap, service, &hints, &res);

	if (rc) {
	    if (debug) {
		fprintf(stderr, "Lookup of \"%s\" failed: %s\n", *ap,
			gai_strerror(rc));
	    }
	    continue;
	}

	for (ai = res; ai != NULL; ai = ai->ai_next) {
	    if (debug) {
		char address[NI_MAXHOST];
		char port[NI_MAXSERV];

		rc = getnameinfo(ai->ai_addr, ai->ai_addrlen, address,
				 sizeof(address), port, sizeof port,
				 NI_NUMERICHOST | NI_NUMERICSERV);

		fprintf(stderr, "Connecting to %s:%s...\n",
			rc ? "unknown" : address,
			rc ? "--" : port);
	    }

	    sd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

	    if (sd < 0) {
		if (debug)
		    fprintf(stderr, "socket() failed: %s\n", strerror(errno));
		continue;
	    }

	    if (connect(sd, ai->ai_addr, ai->ai_addrlen) == 0) {
		freeaddrinfo(res);
		client_freelist(arguments);
		return sd;
	    }

	    if (debug) {
		fprintf(stderr, "Connection failed: %s\n", strerror(errno));
	    }

	    close(sd);
	}

    	freeaddrinfo(res);
    }

    client_freelist(arguments);
    strncpy (response, "no servers available", len_response);
    return NOTOK;
}


/*
 * Free a list of strings
 */

static void
client_freelist(char **list)
{
    free(*list);
}


/*
 * static copies of three nmh subroutines
 */

static char *broken[MAXARGS + 1];

static char **
client_brkstring (char *strg, char *brksep, char *brkterm)
{
    register int bi;
    register char c, *sp;

    sp = strg;

    for (bi = 0; bi < MAXARGS; bi++) {
	while (client_brkany (c = *sp, brksep))
	    *sp++ = 0;
	if (!c || client_brkany (c, brkterm)) {
	    *sp = 0;
	    broken[bi] = 0;
	    return broken;
	}

	broken[bi] = sp;
	while ((c = *++sp) && !client_brkany (c, brksep) && !client_brkany (c, brkterm))
	    continue;
    }
    broken[MAXARGS] = 0;

    return broken;
}


/*
 * returns 1 if chr in strg, 0 otherwise
 */
static int
client_brkany (char chr, char *strg)
{
    register char *sp;

    if (strg)
	for (sp = strg; *sp; sp++)
	    if (chr == *sp)
		return 1;
    return 0;
}


/*
 * copy a string array and return pointer to end
 */
static char **
client_copyip (char **p, char **q, int len_q)
{
    while (*p && --len_q > 0)
	*q++ = *p++;

    *q = NULL;

    return q;
}
