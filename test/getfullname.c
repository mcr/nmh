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

extern void escape_display_name (char *, size_t);

int
main(int argc, char *argv[])
{
	struct passwd *pwd;
	char buf[BUFSIZ], *p;

        if (argc > 2) {
		fprintf (stderr, "usage: %s [name]\n", argv[0]);
		return 1;
	}
	if (argc < 2) {
		pwd = getpwuid(getuid());
		if (! pwd) {
			fprintf(stderr, "Unable to retrieve user info for "
				"userid %ld\n", (long) getuid());
			exit(1);
		}

		strncpy(buf, pwd->pw_gecos, sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';
	} else
		strncpy(buf, argv[1], sizeof(buf));

	/*
	 * Perform the same processing that getuserinfo() does.
	 */

	/*
	 * Stop at the first comma.
	 */
	if ((p = strchr(buf, ',')))
		*p = '\0';

	/*
	 * Quote the entire string if it has a special character in it.
	 */
	escape_display_name (buf, sizeof(buf));

	printf("%s\n", buf);

	exit(0);
}
