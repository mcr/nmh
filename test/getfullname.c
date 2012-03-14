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
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

int
main(int argc, char *argv[])
{
	struct passwd *pwd;
	char name[BUFSIZ], *p;

	if (argc > 1) {
		fprintf (stderr, "usage: %s\n", argv[0]);
	}

	pwd = getpwuid(getuid());

	if (! pwd) {
		fprintf(stderr, "Unable to retrieve user info for "
			"userid %ld\n", (long) getuid());
		exit(1);
	}

	/*
	 * Perform the same processing that getuserinfo() does.
	 */

	strncpy(name, pwd->pw_gecos, sizeof(name));

	name[sizeof(name) - 1] = '\0';

	/*
	 * Stop at the first comma
	 */

	if ((p = strchr(name, ',')))
		*p = '\0';

	/*
	 * Quote the entire string if it has a "." in it
	 */

	if (strchr(name, '.')) {
		char tmp[BUFSIZ];

		snprintf(tmp, sizeof(tmp), "\"%s\"", name);
		strncpy(name, tmp, sizeof(name));

		name[sizeof(name) - 2] = '"';
		name[sizeof(name) - 1] = '\0';
	}

	printf("%s\n", name);

	exit(0);
}
