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
#include <errno.h>


int
client (char *server, char *service, char *response, int len_response,
	int debug)
{
    int sd, rc;
    struct addrinfo hints, *res, *ai;

    if (!server || *server == '\0') {
	snprintf(response, len_response, "Internal error: NULL server name "
		 "passed to client()");
	return NOTOK;
    }

    memset(&hints, 0, sizeof(hints));
#ifdef AI_ADDRCONFIG
    hints.ai_flags = AI_ADDRCONFIG;
#endif
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (debug) {
	fprintf(stderr, "Trying to connect to \"%s\" ...\n", server);
    }

    rc = getaddrinfo(server, service, &hints, &res);

    if (rc) {
	snprintf(response, len_response, "Lookup of \"%s\" failed: %s",
		 server, gai_strerror(rc));
	return NOTOK;
    }

    for (ai = res; ai != NULL; ai = ai->ai_next) {
	if (debug) {
	    char address[NI_MAXHOST];
	    char port[NI_MAXSERV];

	    rc = getnameinfo(ai->ai_addr, ai->ai_addrlen, address,
			     sizeof(address), port, sizeof port,
			     NI_NUMERICHOST | NI_NUMERICSERV);

	    fprintf(stderr, "Connecting to %s:%s...\n",
		    rc ? "unknown" : address, rc ? "--" : port);
	}

	sd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

	if (sd < 0) {
	    if (debug)
		fprintf(stderr, "socket() failed: %s\n", strerror(errno));
	    rc = errno;
	    continue;
	}

	if (connect(sd, ai->ai_addr, ai->ai_addrlen) == 0) {
	    freeaddrinfo(res);
	    return sd;
	}

	rc = errno;

	if (debug) {
	    fprintf(stderr, "Connection failed: %s\n", strerror(errno));
	}

	close(sd);
    }

    /*
     * Improve error handling a bit. If we were given multiple IP addresses
     * then return the old "no servers available" error, but point the user
     * to -snoop (hopefully that's universal).  Otherwise report a specific
     * error.
     */

    if (res->ai_next)
	snprintf(response, len_response, "no servers available (use -snoop "
		 "for details");
    else
	snprintf(response, len_response, "Connection to \"%s\" failed: %s",
		 server, strerror(errno));

    freeaddrinfo(res);

    return NOTOK;
}
