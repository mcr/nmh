/* getcanon.c - Print the canonical name of a host, default to localhost.
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>


int
main(int argc, char *argv[])
{
  char buf[_POSIX_HOST_NAME_MAX + 1];
  const char *hostname;
  struct addrinfo hints, *res;

  /* Borrowed the important code below from LocalName() in sbr/mts.c. */

  if (argc > 2) {
    fprintf(stderr, "usage: %s [hostname]\n", argv[0]);
    return 1;
  }
  if (argc == 2)
    hostname = argv[1];
  else {
    /* First get our local name. */
    if (gethostname(buf, sizeof buf)) {
        fprintf(stderr, "gethostname() failed: %s\n", strerror(errno));
        return 1;
    }
    hostname = buf;
  }

  /* Now fully qualify the hostname. */
  memset(&hints, 0, sizeof hints);
  hints.ai_flags = AI_CANONNAME;
  hints.ai_family = AF_UNSPEC;

  if (getaddrinfo(hostname, NULL, &hints, &res)) {
    puts(hostname);
    return 1;
  }
  puts(res->ai_canonname);

  return 0;
}
