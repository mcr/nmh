/* context_read.c -- find and read profile and context files
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 *
 *	This function must be called early on in any nmh utility, and
 *	may only be called once.  It does the following:
 *
 *	 o  Sets the global variable "mypath" to the home directory path.
 *
 *	 o  Sets the global variable "defpath" to the absolute path of
 *	    the profile file.
 *
 *	 o  Reads in the profile file.  Bails out if it can't.
 *
 *	 o  Makes sure that the mail directory exists, prompting for
 *	    creation if it doesn't.
 *
 *	 o  Reads the context file either as set by the MHCONTEXT
 *	    environment variable or by the profile.
 */

#include <h/mh.h>				/* mh internals */
#include "lock_file.h"
#include "m_maildir.h"
#include "makedir.h"
#include <pwd.h>				/* structure for getpwuid() results */

void
context_read (void)
{
    char			buf[BUFSIZ];	/* path name buffer */
    char			*cp;		/* miscellaneous pointer */
    char			*nd;		/* nmh directory pointer */
    struct	stat		st;		/* stat() results */
    struct	passwd	        *pw;		/* getpwuid() results */
    FILE        		*ib;		/* profile and context file pointer */
    int failed_to_lock = 0;

    /*
     *  If this routine _is_ called again (despite the warnings in the
     *  comments above), return immediately.
     */
    if ( m_defs != 0 )
        return;

    /*
     *	Find user's home directory.  Try the HOME environment variable first,
     *	the home directory field in the password file if that's not found.
     */

    if ((mypath = getenv("HOME")) == NULL) {
	if ((pw = getpwuid(getuid())) == NULL || *pw->pw_dir == '\0')
	    adios(NULL, "cannot determine your home directory");
        mypath = pw->pw_dir;
    }

    /*
     *	Find and read user's profile.  Check for the existence of an MH environment
     *	variable first with non-empty contents.  Convert any relative path name
     *	found there to an absolute one.  Look for the profile in the user's home
     *	directory if the MH environment variable isn't set.
     */

    if ((cp = getenv("MH")) && *cp != '\0') {
	defpath = path(cp, TFILE);

        /* defpath is an absolute path; make sure that always MH is, too. */
	setenv("MH", defpath, 1);
	if (stat(defpath, &st) != -1 && (st.st_mode & S_IFREG) == 0)
		adios(NULL, "`%s' specified by your MH environment variable is not a normal file", cp);

	if ((ib = fopen(defpath, "r")) == NULL)
	    adios(NULL, "unable to read the `%s' profile specified by your MH environment variable", defpath);
    }
    else {
	defpath = concat(mypath, "/", mh_profile, NULL);

	if ((ib = fopen(defpath, "r")) == NULL)
	    adios(NULL, "Doesn't look like nmh is installed.  Run install-mh to do so.");

	cp = mh_profile;
    }

    readconfig (&m_defs, ib, cp, 0);
    fclose (ib);

    /*
     *	Find the user's nmh directory, which is specified by the "path" profile component.
     *	Convert a relative path name to an absolute one rooted in the home directory.
     */

    if ((cp = context_find ("path")) == NULL)
	adios(NULL, "Your %s file does not contain a path entry.", defpath);

    if (*cp == '\0')
	adios(NULL, "Your `%s' profile file does not contain a valid path entry.", defpath);

    if (*cp != '/')
	(void)snprintf (nd = buf, sizeof(buf), "%s/%s", mypath, cp);
    else
	nd = cp;

    if (stat(nd, &st) == -1) {
	if (errno != ENOENT)
	    adios (nd, "error opening");

	cp = concat ("Your MH-directory \"", nd, "\" doesn't exist; Create it? ", NULL);

	if (!read_yes_or_no_if_tty(cp))
	    adios (NULL, "unable to access MH-directory \"%s\"", nd);

	free (cp);

	if (!makedir (nd))
	    adios (NULL, "unable to create %s", nd);
    }

    else if ((st.st_mode & S_IFDIR) == 0)
	adios (NULL, "`%s' is not a directory", nd);

    /*
     *	Open and read user's context file.  The name of the context file comes from the
     *	profile unless overridden by the MHCONTEXT environment variable.
     */

    if ((cp = getenv ("MHCONTEXT")) == NULL || *cp == '\0')
	cp = context;

    /* context is NULL if context_foil() was called to disable use of context
     * We also support users setting explicitly setting MHCONTEXT to /dev/null.
     * (if this wasn't special-cased then the locking would be liable to fail)
     */
    if (!cp || (strcmp(cp,"/dev/null") == 0)) {
	ctxpath = NULL;
	return;
    }
    
    ctxpath = getcpy (m_maildir (cp));

    if ((ib = lkfopendata (ctxpath, "r", &failed_to_lock))) {
	readconfig(NULL, ib, cp, 1);
	lkfclosedata (ib, ctxpath);
    }
}
