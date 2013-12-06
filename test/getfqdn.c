/*
 * getfqdn.c - Print the FQDN of a host, default to localhost.
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <netdb.h>   /* for getaddrinfo */
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>  /* for gethostname */
#include <limits.h>  /* for _POSIX_HOST_NAME_MAX */
#include <string.h>  /* for memset */
#include <stdio.h>
#include <errno.h>


int
main(int argc, char *argv[])
{
  char buf[_POSIX_HOST_NAME_MAX + 1];
  const char *hostname = buf;
  struct addrinfo hints, *res;
  int status = 0;

  /* Borrowed the important code below from LocalName() in sbr/mts.c. */

  if (argc < 2) {
    /* First get our local name. */
    status = gethostname(buf, sizeof buf);
  } else if (argc == 2) {
    hostname = argv[1];
  } else if (argc > 2) {
    fprintf (stderr, "usage: %s [hostname]\n", argv[0]);
    return 1;
  }

  if (status == 0) {
    /* Now fully qualify the hostname. */
    memset(&hints, 0, sizeof hints);
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = AF_UNSPEC;

    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) == 0) {
      printf ("%s\n", res->ai_canonname);
      freeaddrinfo(res);
    } else {
      printf("%s\n", hostname);
    }
  } else {
    fprintf(stderr, "gethostname() failed: %s\n", strerror(errno));
  }

  return status;
}
