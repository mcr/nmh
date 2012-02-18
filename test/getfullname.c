/*
 * getfullname.c - Extract a user's name out of the GECOS field in
 *		   the password file.
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

int
main(int argc, char *argv[])
{
	if (argc > 1) {
		fprintf (stderr, "usage: %s\n", argv[0]);
	}

	struct passwd *pwd;

	pwd = getpwuid(getuid());

	if (! pwd) {
		fprintf(stderr, "Unable to retrieve user info for "
			"userid %d\n", getuid());
		exit(1);
	}

	printf("%s\n", pwd->pw_gecos);

	exit(0);
}
