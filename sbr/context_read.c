
/*
 * context_read.c -- find and read profile and context files
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <errno.h>
#include <pwd.h>

extern int errno;

void
context_read (void)
{
    pid_t pid;
    register char *cp, *pp;
    char buf[BUFSIZ];
    struct stat st;
    register struct passwd *pw;
    register FILE *ib;

    if (defpath)
	return;

    /*
     * Find user's home directory
     */
    if (!mypath) {
	if ((mypath = getenv ("HOME")))
	    mypath = getcpy (mypath);
	else
	    if ((pw = getpwuid (getuid ())) == NULL
		    || pw->pw_dir == NULL
		    || *pw->pw_dir == 0)
		adios (NULL, "no HOME envariable");
	    else
		mypath = getcpy (pw->pw_dir);
	if ((cp = mypath + strlen (mypath) - 1) > mypath && *cp == '/')
	    *cp = 0;
    }

    /*
     * open and read user's profile
     */
    if ((cp = getenv ("MH")) && *cp != '\0') {
	defpath = path (cp, TFILE);
	if ((ib = fopen (defpath, "r")) == NULL)
	    adios (defpath, "unable to read");
	if (*cp != '/')
	    m_putenv ("MH", defpath);
    } else {
	defpath = concat (mypath, "/", mh_profile, NULL);

	if ((ib = fopen (defpath, "r")) == NULL) {
	    switch (pid = vfork ()) {
		case -1:
		    adios ("fork", "unable to");

		case 0:
		    setgid (getgid ());
		    setuid (getuid ());

		    execlp (installproc, "install-mh", "-auto", NULL);
		    fprintf (stderr, "unable to exec ");
		    perror (installproc);
		    _exit (-1);

		default:
		    if (pidwait (pid, 0)
			    || (ib = fopen (defpath, "r")) == NULL)
			adios (NULL, "[install-mh aborted]");
	    }
	}
    }
    readconfig (&m_defs, ib, mh_profile, 0);
    fclose (ib);

    /*
     * Find user's nmh directory
     */
    if ((pp = context_find ("path")) && *pp != '\0') {
	if (*pp != '/')
	    snprintf (buf, sizeof(buf), "%s/%s", mypath, pp);
	else
	    strncpy (buf, pp, sizeof(buf));
	if (stat(buf, &st) == -1) {
	    if (errno != ENOENT)
		adios (buf, "error opening");
	    cp = concat ("Your MH-directory \"", buf,
		"\" doesn't exist; Create it? ", NULL);
	    if (!getanswer(cp))
		adios (NULL, "unable to access MH-directory \"%s\"", buf);
	    free (cp);
	    if (!makedir (buf))
		adios (NULL, "unable to create", buf);
	}
    }

    /*
     * open and read user's context file
     */
    if (!(cp = getenv ("MHCONTEXT")) || *cp == '\0')
	cp = context;
    ctxpath = getcpy (m_maildir (cp));
    if ((ib = fopen (ctxpath, "r"))) {
	readconfig ((struct node **) 0, ib, cp, 1);
	fclose (ib);
    }
}
