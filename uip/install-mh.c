/* install-mh.c -- initialize the nmh environment of a new user
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"				/* mh internals */
#include "sbr/error.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/m_maildir.h"
#include "sbr/makedir.h"
#include <pwd.h>				/* structure for getpwuid() results */
#include "sbr/read_line.h"

#define INSTALLMH_SWITCHES \
    X("auto", 0, AUTOSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("check", 1, CHECKSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(INSTALLMH);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(INSTALLMH, switches);
#undef X


int
main (int argc, char **argv)
{
    bool autof = false;
    char *cp, buf[BUFSIZ];
    const char *pathname;
    char *dp, **arguments, **argp;
    struct node *np;
    struct passwd *pw;
    struct stat st;
    FILE *in, *out;
    bool check;

    if (nmh_init(argv[0], false, false)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 0);
    argp = arguments;

    check = false;

    while ((dp = *argp++)) {
	if (*dp == '-') {
	    switch (smatch (++dp, switches)) {
		case AMBIGSW:
		    ambigsw (dp, switches);
		    done (1);
		case UNKWNSW:
		    die("-%s unknown\n", dp);

		case HELPSW:
		    snprintf (buf, sizeof(buf), "%s [switches]", invo_name);
		    print_help (buf, switches, 0);
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

		case AUTOSW:
		    autof = true;
		    continue;

		case CHECKSW:
		    check = true;
		    continue;
	    }
	} else {
	    die("%s is invalid argument", dp);
	}
    }

    /*
     *	Find user's home directory.  Try the HOME environment variable first,
     *	the home directory field in the password file if that's not found.
     */

    if ((mypath = getenv("HOME")) == NULL) {
	if ((pw = getpwuid(getuid())) == NULL || *pw->pw_dir == '\0')
	    die("cannot determine your home directory");
        mypath = pw->pw_dir;
    }

    /*
     *	Find the user's profile.  Check for the existence of an MH environment
     *	variable first with non-empty contents.  Convert any relative path name
     *	found there to an absolute one.  Look for the profile in the user's home
     *	directory if the MH environment variable isn't set.
     */

    if ((cp = getenv("MH")) && *cp != '\0')
	defpath = path(cp, TFILE);
    else
	defpath = concat(mypath, "/", mh_profile, NULL);

    /*
     *	Check for the existence of the profile file.  It's an error if it exists and
     *	this isn't an installation check.  An installation check fails if it does not
     *	exist, succeeds if it does.
     */

    if (stat (defpath, &st) != NOTOK) {
	if (check)
	    done(0);
	if (autof)
	    die("invocation error");
        die("You already have an nmh profile, use an editor to modify it");
    }
    if (check)
	done(1);

    if (!autof && read_switch ("Do you want help? ", anoyes)) {
	(void)printf(
	 "\n"
	 "Prior to using nmh, it is necessary to have a file in your login\n"
	 "directory (%s) named %s which contains information\n"
	 "to direct certain nmh operations.  The only item which is required\n"
	 "is the path to use for all nmh folder operations.  The suggested nmh\n"
	 "path for you is %s/Mail...\n"
	 "\n", mypath, mh_profile, mypath);
    }

    cp = concat (mypath, "/", "Mail", NULL);
    if (stat (cp, &st) != NOTOK) {
	if (!S_ISDIR(st.st_mode))
	    goto query;
        cp = concat ("You already have the standard nmh directory \"",
                cp, "\".\nDo you want to use it for nmh? ", NULL);
        if (!read_switch (cp, anoyes))
            goto query;
        pathname = "Mail";
    } else {
	if (autof)
	    puts("I'm going to create the standard nmh path for you.");
	else
	    cp = concat ("Do you want the standard nmh path \"",
		    mypath, "/", "Mail\"? ", NULL);
	if (autof || read_switch (cp, anoyes))
	    pathname = "Mail";
	else {
query:
	    if (read_switch ("Do you want a path below your login directory? ",
			anoyes)) {
		printf ("What is the path?  %s/", mypath);
		pathname = read_line ();
		if (pathname == NULL) done (1);
	    } else {
		fputs("What is the whole path?  /", stdout);
		pathname = read_line ();
		if (pathname == NULL) done (1);
		pathname = concat ("/", pathname, NULL);
	    }
	}
    }

    if (chdir (mypath) < 0) {
	advise (mypath, "chdir");
    }
    if (chdir (pathname) == NOTOK) {
	cp = concat ("\"", pathname, "\" doesn't exist; Create it? ", NULL);
	if (autof || read_switch (cp, anoyes))
	    if (makedir (pathname) == 0)
		die("unable to create %s", pathname);
    } else {
	puts("[Using existing directory]");
    }

    /*
     * Add some initial elements to the profile/context list
     */
    NEW(np);
    m_defs = np;
    np->n_name = mh_xstrdup("Path");
    np->n_field = mh_xstrdup(pathname);
    np->n_context = 0;
    np->n_next = NULL;

    /*
     * If there is a default profile file in the
     * nmh `etc' directory, then read it also.
     */
    if ((in = fopen (mh_defaults, "r"))) {
	readconfig (&np->n_next, in, mh_defaults, 0);
	fclose (in);
    }

    ctxpath = mh_xstrdup(m_maildir(context = "context"));

    /* Initialize current folder to default */
    context_replace (pfolder, defaultfolder);
    context_save ();

    /*
     * Now write out the initial .mh_profile
     */
    if ((out = fopen (defpath, "w")) == NULL)
	adios (defpath, "unable to write");
    /*
     * The main purpose of this first line is to fool file(1).
     * Without it, if the first line of the profile is Path:,
     * file 5.19 reports its type as message/news.  With it,
     * it reports the type as text/plain.
     */
    fprintf (out, "MH-Profile-Version: 1.0\n");
    for (np = m_defs; np; np = np->n_next) {
	if (!np->n_context)
	    fprintf (out, "%s: %s\n", np->n_name, np->n_field);
    }
    fclose (out);

    puts ("\nPlease see nmh(7) for an introduction to nmh.\n");
    print_intro (stdout, false);

    /* Initialize the saved nmh version.  The Path profile entry was added
       above, that's all this needs. */
    (void) nmh_version_changed (0);

    done (0);
    return 1;
}
