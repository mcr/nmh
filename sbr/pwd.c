
/*
 * pwd.c -- return the current working directory
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

static char curwd[PATH_MAX];


char *
pwd(void)
{
    register char *cp;

    if (!getcwd (curwd, PATH_MAX)) {
	admonish (NULL, "unable to determine working directory");
	if (!mypath || !*mypath
		|| (strcpy (curwd, mypath), chdir (curwd)) == -1) {
	    strcpy (curwd, "/");
	    chdir (curwd);
	}
	return curwd;
    }

    if ((cp = curwd + strlen (curwd) - 1) > curwd && *cp == '/')
	*cp = '\0';

    return curwd;
}


#if 0

/*
 * Currently commented out.  Everyone seems
 * to have a native version these days.
 */

/*
 * getwd() - get the current working directory
 */

int
getwd(char *cwd)
{
    int found;
    char tmp1[BUFSIZ], tmp2[BUFSIZ];
    struct stat st1, st2, root;
    register struct direct *dp;
    register DIR *dd;

    strcpy (cwd, "/");
    stat ("/", &root);

    for (;;) {
	if ((dd = opendir ("..")) == NULL)
	    return -1;
	if (stat (".", &st2) == -1 || stat ("..", &st1) == -1)
	    goto out;
	if (st2.st_ino == root.st_ino && st2.st_dev == root.st_dev) {
	    closedir (dd);
	    return chdir (cwd);
	}

	if (st2.st_ino == st1.st_ino && st2.st_dev == st1.st_dev) {
	    closedir (dd);
	    chdir ("/");
	    if ((dd = opendir (".")) == NULL)
		return -1;
	    if (stat (".", &st1) < 0)
		goto out;
	    if (st2.st_dev != st1.st_dev)
		while (dp = readdir (dd)) {
		    if (stat (dp->d_name, &st1) == -1)
			goto out;
		    if (st2.st_dev == st1.st_dev) {
			snprintf (tmp1, sizeof(tmp1), "%s%s", dp->d_name, cwd);
			strcpy (cwd + 1, tmp1);
			closedir (dd);
			return (chdir (cwd));
		    }
		}
	    else {
		closedir (dd);
		return (chdir (cwd));
	    }
	}

	found = 0;
	while (dp = readdir (dd)) {
	    snprintf (tmp2, sizeof(tmp2), "../%s", dp->d_name);
	    if (stat (tmp2, &st1) != -1
		    && st1.st_ino == st2.st_ino
		    && st1.st_dev == st2.st_dev) {
		closedir (dd);
		found++;
		chdir ("..");
		snprintf (tmp1, sizeof(tmp1), "%s%s", dp->d_name, cwd);
		strcpy (cwd + 1, tmp1);
		break;
	    }
	}
	if (!found)
	    goto out;
    }

out: ;
    closedir (dd);
    return -1;
}

#endif
