
/*
 * whatnowsbr.c -- the WhatNow shell
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 *
 *  Several options have been added to ease the inclusion of attachments
 *  using the header field name mechanism added to anno and send.  The
 *  -attach option is used to specify the header field name for attachments.
 *
 *  Several commands have been added at the whatnow prompt:
 *
 *	cd [ directory ]	This option works just like the shell's
 *				cd command and lets the user change the
 *				directory from which attachments are
 *				taken so that long path names are not
 *				needed with every file.
 *
 *	ls [ ls-options ]	This option works just like the normal
 *				ls command and exists to allow the user
 *				to verify file names in the directory.
 *
 *	pwd			This option works just like the normal
 *				pwd command and exists to allow the user
 *				to verify the directory.
 *
 *	attach [-v] files	This option attaches the named files to
 *				the draft.  -v displays the mhbuild
 *				directive that send(1) will use.
 *
 *	alist [-ln]		This option lists the attachments on the
 *				draft.  -l gets long listings, -n gets
 *				numbered listings.
 *
 *	detach files		This option removes attachments from the
 *	detach -n numbers	draft.  This can be done by file name or
 *				by attachment number.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/mime.h>
#include <h/utils.h>

#ifdef OAUTH_SUPPORT
# include <h/oauth.h>
#endif

#define WHATNOW_SWITCHES \
    X("draftfolder +folder", 0, DFOLDSW) \
    X("draftmessage msg", 0, DMSGSW) \
    X("nodraftfolder", 0, NDFLDSW) \
    X("editor editor", 0, EDITRSW) \
    X("noedit", 0, NEDITSW) \
    X("prompt string", 4, PRMPTSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("attach header-field-name", -6, ATTACHSW) \
    X("noattach", -8, NOATTACHSW) \


#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(WHATNOW);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(WHATNOW, whatnowswitches);
#undef X

/*
 * Options at the "whatnow" prompt
 */
#define PROMPT_SWITCHES \
    X("edit [<editor> <switches>]", 0, EDITSW) \
    X("refile [<switches>] +folder", 0, REFILEOPT) \
    X("mime [<switches>]", 0, BUILDMIMESW) \
    X("display [<switches>]", 0, DISPSW) \
    X("list [<switches>]", 0, LISTSW) \
    X("send [<switches>]", 0, SENDSW) \
    X("push [<switches>]", 0, PUSHSW) \
    X("whom [<switches>]", 0, WHOMSW) \
    X("quit [-delete]", 0, QUITSW) \
    X("delete", 0, DELETESW) \
    X("cd [directory]", 0, CDCMDSW) \
    X("pwd", 0, PWDCMDSW) \
    X("ls", 2, LSCMDSW) \
    X("attach [-v]", 0, ATTACHCMDSW) \
    X("detach [-n]", 0, DETACHCMDSW) \
    X("alist [-ln] ", 2, ALISTCMDSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(PROMPT);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(PROMPT, aleqs);
#undef X

static char *myprompt = "\nWhat now? ";

/*
 * static prototypes
 */
static int editfile (char **, char **, char *, int, struct msgs *,
	char *, char *, int, int);
static int sendfile (char **, char *, int);
static void sendit (char *, char **, char *, int);
static int buildfile (char **, char *);
static int whomfile (char **, char *);
static int removefile (char *);
static int checkmimeheader (char *);
static void writelscmd(char *, int, char *, char **);
static void writesomecmd(char *buf, int bufsz, char *cmd, char *trailcmd, char **argp);
static FILE* popen_in_dir(const char *dir, const char *cmd, const char *type);
static int system_in_dir(const char *dir, const char *cmd);
static int copyf (char *, char *);


int
WhatNow (int argc, char **argv)
{
    int isdf = 0, nedit = 0, use = 0, atfile = 1;
    char *cp, *dfolder = NULL, *dmsg = NULL;
    char *ed = NULL, *drft = NULL, *msgnam = NULL;
    char buf[BUFSIZ], prompt[BUFSIZ];
    char **argp, **arguments;
    struct stat st;
    char	cwd[PATH_MAX + 1];	/* current working directory */
    char	file[PATH_MAX + 1];	/* file name buffer */
    char	shell[PATH_MAX + 1];	/* shell response buffer */
    FILE	*f;			/* read pointer for bgnd proc */
    char	*l;			/* set on -l to alist  command */
    int		n;			/* set on -n to alist command */

    /* Need this if called from what_now(). */
    invo_name = r1bindex (argv[0], '/');

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /*
     *	Get the initial current working directory.
     */

    if (getcwd(cwd, sizeof (cwd)) == NULL) {
	adios("getcwd", "could not get working directory");
    }

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, whatnowswitches)) {
	    case AMBIGSW:
		ambigsw (cp, whatnowswitches);
		done (1);
	    case UNKWNSW:
		adios (NULL, "-%s unknown", cp);

	    case HELPSW:
		snprintf (buf, sizeof(buf), "%s [switches] [file]", invo_name);
		print_help (buf, whatnowswitches, 1);
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case DFOLDSW:
		if (dfolder)
		    adios (NULL, "only one draft folder at a time!");
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		dfolder = path (*cp == '+' || *cp == '@' ? cp + 1 : cp,
				*cp != '@' ? TFOLDER : TSUBCWF);
		continue;
	    case DMSGSW:
		if (dmsg)
		    adios (NULL, "only one draft message at a time!");
		if (!(dmsg = *argp++) || *dmsg == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;
	    case NDFLDSW:
		dfolder = NULL;
		isdf = NOTOK;
		continue;

	    case EDITRSW:
		if (!(ed = *argp++) || *ed == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		nedit = 0;
		continue;
	    case NEDITSW:
		nedit++;
		continue;

	    case PRMPTSW:
		if (!(myprompt = *argp++) || *myprompt == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;

	    case ATTACHSW:
		advise(NULL, "The -attach switch is deprecated");
		continue;

	    case NOATTACHSW:
		advise(NULL, "The -noattach switch is deprecated");
		continue;
	    }
	}
	if (drft)
	    adios (NULL, "only one draft at a time!");
	else
	    drft = cp;
    }

    if ((drft == NULL && (drft = getenv ("mhdraft")) == NULL) || *drft == 0)
	drft = getcpy (m_draft (dfolder, dmsg, 1, &isdf));

    msgnam = (cp = getenv ("mhaltmsg")) && *cp ? mh_xstrdup(cp) : NULL;

    if ((cp = getenv ("mhatfile")) && *cp)
    	atfile = atoi(cp);

    if ((cp = getenv ("mhuse")) && *cp)
	use = atoi (cp);

    if (ed == NULL && ((ed = getenv ("mheditor")) == NULL || *ed == 0)) {
	ed = NULL;
	nedit++;
    }

    /* start editing the draft, unless -noedit was given */
    if (!nedit && editfile (&ed, NULL, drft, use, NULL, msgnam,
    			    NULL, 1, atfile) < 0)
	done (1);

    snprintf (prompt, sizeof(prompt), myprompt, invo_name);
    for (;;) {
#ifdef READLINE_SUPPORT
	if (!(argp = read_switch_multiword_via_readline (prompt, aleqs))) {
#else /* ! READLINE_SUPPORT */
	if (!(argp = read_switch_multiword (prompt, aleqs))) {
#endif /* READLINE_SUPPORT */
	    (void) m_unlink (LINK);
	    done (1);
	}
	switch (smatch (*argp, aleqs)) {
	case DISPSW:
	    /* display the message being replied to, or distributed */
	    if (msgnam)
		showfile (++argp, msgnam);
	    else
		advise (NULL, "no alternate message to display");
	    break;

	case BUILDMIMESW:
	    /* Translate MIME composition file */
	    buildfile (++argp, drft);
	    break;

	case EDITSW:
	    /* Call an editor on the draft file */
	    if (*++argp)
		ed = *argp++;
	    if (editfile (&ed, argp, drft, NOUSE, NULL, msgnam,
	    		  NULL, 1, atfile) == NOTOK)
		done (1);
	    break;

	case LISTSW:
	    /* display the draft file */
	    showfile (++argp, drft);
	    break;

	case WHOMSW:
	    /* Check to whom the draft would be sent */
	    whomfile (++argp, drft);
	    break;

	case QUITSW:
	    /* Quit, and possibly delete the draft */
	    if (*++argp && (*argp[0] == 'd' ||
		((*argp)[0] == '-' && (*argp)[1] == 'd'))) {
		removefile (drft);
	    } else {
		if (stat (drft, &st) != NOTOK)
		    advise (NULL, "draft left on %s", drft);
	    }
	    done (1);

	case DELETESW:
	    /* Delete draft and exit */
	    removefile (drft);
	    done (1);

	case PUSHSW:
	    /* Send draft in background */
	    if (sendfile (++argp, drft, 1))
		done (1);
	    break;

	case SENDSW:
	    /* Send draft */
	    sendfile (++argp, drft, 0);
	    break;

	case REFILEOPT:
	    /* Refile the draft */
	    if (refile (++argp, drft) == 0)
		done (0);
	    break;

	case CDCMDSW:
	    /* Change the working directory for attachments
	     *
	     *	Run the directory through the user's shell so that
	     *	we can take advantage of any syntax that the user
	     *	is accustomed to.  Read back the absolute path.
	     */

	    if (*(argp+1) == NULL) {
		strcpy(buf, "$SHELL -c \"cd&&pwd\"");
	    }
	    else {
		writesomecmd(buf, BUFSIZ, "cd", "pwd", argp);
	    }
	    if ((f = popen_in_dir(cwd, buf, "r")) != NULL) {
		if (fgets(cwd, sizeof (cwd), f) == NULL) {
		    advise (buf, "fgets");
		}
                TrimSuffixC(cwd, '\n');
		pclose(f);
	    }
	    else {
		advise("popen", "could not get directory");
	    }

	    break;

	case PWDCMDSW:
	    /* Print the working directory for attachments */
	    printf("%s\n", cwd);
	    break;

	case LSCMDSW:
	    /* List files in the current attachment working directory
	     *
	     *	Use the user's shell so that we can take advantage of any
	     *	syntax that the user is accustomed to.
	     */
	    writelscmd(buf, sizeof(buf), "", argp);
	    (void)system_in_dir(cwd, buf);
	    break;

	case ALISTCMDSW:
	    /*
	     *	List attachments on current draft.  Options are:
	     *
	     *	 -l	long listing (full path names)
	     *	 -n	numbers listing
	     */

	    if (checkmimeheader(drft))
		break;

	    l = NULL;
	    n = 0;

	    while (*++argp != NULL) {
		if (strcmp(*argp, "-l") == 0)
		    l = "/";

		else if (strcmp(*argp, "-n") == 0)
		    n = 1;

		else if (strcmp(*argp, "-ln") == 0 || strcmp(*argp, "-nl") == 0) {
		    l = "/";
		    n = 1;
		}

		else {
		    n = -1;
		    break;
		}
	    }

	    if (n == -1)
		advise(NULL, "usage is alist [-ln].");

	    else
		annolist(drft, ATTACH_FIELD, l, n);

	    break;

	case ATTACHCMDSW: {
	    /*
	     *	Attach files to current draft.
	     */

            int verbose = 0;
            char **ap;

	    if (checkmimeheader(drft))
		break;

	    for (ap = argp+1; *ap; ++ap) {
		if (strcmp(*ap, "-v") == 0) {
		    ++argp;
		    verbose = 1;
		} else if (*ap[0] != '-') {
		    break;
		}
	    }

	    if (*(argp+1) == NULL) {
		advise(NULL, "attach command requires file argument(s).");
		break;
	    }

	    /*
	     *	Build a command line that causes the user's shell to list the file name
	     *	arguments.  This handles and wildcard expansion, tilde expansion, etc.
	     */
	    writelscmd(buf, sizeof(buf), "-d --", argp);

	    /*
	     *	Read back the response from the shell, which contains a number of lines
	     *	with one file name per line.  Remove off the newline.  Determine whether
	     *	we have an absolute or relative path name.  Prepend the current working
	     *	directory to relative path names.  Add the attachment annotation to the
	     *	draft.
	     */

	    if ((f = popen_in_dir(cwd, buf, "r")) != NULL) {
		while (fgets(shell, sizeof (shell), f) != NULL) {
		    char *ctype;

                    TrimSuffixC(shell, '\n');

		    if (*shell == '/') {
		    	strncpy(file, shell, sizeof(file));
			file[sizeof(file) - 1] = '\0';
		    } else {
			snprintf(file, sizeof(file), "%s/%s", cwd, shell);
		    }

		    annotate(drft, ATTACH_FIELD, file, 1, 0, -2, 1);
		    if (verbose) {
			ctype = mime_type(file);
			printf ("Attaching %s as a %s\n", file, ctype);
			free (ctype);
		    }
		}

		pclose(f);
	    }
	    else {
		advise("popen", "could not get file from shell");
	    }

	    break;
	}
	case DETACHCMDSW:
	    /*
	     *	Detach files from current draft.
	     */

	    /*
	     *	Scan the arguments for a -n.  Mixed file names and numbers aren't allowed,
	     *	so this catches a -n anywhere in the argument list.
	     */

	    if (checkmimeheader(drft))
		break;

	    for (n = 0, arguments = argp + 1; *arguments != NULL; arguments++) {
		if (strcmp(*arguments, "-n") == 0) {
			n = 1;
			break;
		}
	    }

	    /*
	     *	A -n was found so interpret the arguments as attachment numbers.
	     *	Decrement any remaining argument number that is greater than the one
	     *	just processed after processing each one so that the numbering stays
	     *	correct.
	     */

	    if (n == 1) {
		for (arguments = argp + 1; *arguments != NULL; arguments++) {
		    if (strcmp(*arguments, "-n") == 0)
			continue;

		    if (**arguments != '\0') {
			n = atoi(*arguments);
			annotate(drft, ATTACH_FIELD, NULL, 1, 0, n, 1);

			for (argp = arguments + 1; *argp != NULL; argp++) {
			    if (atoi(*argp) > n) {
				if (atoi(*argp) == 1)
				    *argp = "";
				else
				    (void)sprintf(*argp, "%d", atoi(*argp) - 1);
			    }
			}
		    }
		}
	    }

	    /*
	     *	The arguments are interpreted as file names.  Run them through the
	     *	user's shell for wildcard expansion and other goodies.  Do this from
	     *	the current working directory if the argument is not an absolute path
	     *	name (does not begin with a /).
	     *
	     * We feed all the file names to the shell at once, otherwise you can't
	     * provide a file name with a space in it.
	     */
	    writelscmd(buf, sizeof(buf), "-d --", argp);
	    if ((f = popen_in_dir(cwd, buf, "r")) != NULL) {
		while (fgets(shell, sizeof (shell), f) != NULL) {
                    TrimSuffixC(shell, '\n');
		    annotate(drft, ATTACH_FIELD, shell, 1, 0, 0, 1);
		}
		pclose(f);
	    } else {
		advise("popen", "could not get file from shell");
	    }

	    break;

	default:
	    /* Unknown command */
	    advise (NULL, "say what?");
	    break;
	}
    }
    /*NOTREACHED*/
}



/* Build a command line of the form $SHELL -c "cd 'cwd'; cmd argp ... ; trailcmd". */
static void
writesomecmd(char *buf, int bufsz, char *cmd, char *trailcmd, char **argp)
{
    char *cp;
    /* Note that we do not quote -- the argp from the user
     * is assumed to be quoted as they desire. (We can't treat
     * it as pure literal as that would prevent them using ~,
     * wildcards, etc.) The buffer produced by this function
     * should be given to popen_in_dir() or system_in_dir() so
     * that the current working directory is set correctly.
     */
    int ln = snprintf(buf, bufsz, "$SHELL -c \"%s", cmd);
    /* NB that some snprintf() return -1 on overflow rather than the
     * new C99 mandated 'number of chars that would have been written'
     */
    /* length checks here and inside the loop allow for the
     * trailing "&&", trailcmd, '"' and NUL
     */
    int trailln = strlen(trailcmd) + 4;
    if (ln < 0 || ln + trailln > bufsz)
	adios(NULL, "arguments too long");

    cp = buf + ln;

    while (*argp  &&  *++argp) {
	ln = strlen(*argp);
	/* +1 for leading space */
	if (ln + trailln + 1 > bufsz - (cp-buf))
	    adios(NULL, "arguments too long");
	*cp++ = ' ';
	memcpy(cp, *argp, ln+1);
	cp += ln;
    }
    if (*trailcmd) {
	*cp++ = '&'; *cp++ = '&';
	strcpy(cp, trailcmd);
	cp += trailln - 4;
    }
    *cp++ = '"';
    *cp = 0;
}

/*
 * Build a command line that causes the user's shell to list the file name
 * arguments.  This handles and wildcard expansion, tilde expansion, etc.
 */
static void
writelscmd(char *buf, int bufsz, char *lsoptions, char **argp)
{
  char *lscmd = concat ("ls ", lsoptions, NULL);
  writesomecmd(buf, bufsz, lscmd, "", argp);
  free (lscmd);
}

/* Like system(), but run the command in directory dir.
 * This assumes the program is single-threaded!
 */
static int
system_in_dir(const char *dir, const char *cmd)
{
    char olddir[BUFSIZ];
    int r;

    /* ensure that $SHELL exists, as the cmd was written relying on
       a non-blank $SHELL... */
    setenv("SHELL","/bin/sh",0); /* don't overwrite */

    if (getcwd(olddir, sizeof(olddir)) == 0)
	adios("getcwd", "could not get working directory");
    if (chdir(dir) != 0)
	adios("chdir", "could not change working directory");
    r = system(cmd);
    if (chdir(olddir) != 0)
	adios("chdir", "could not change working directory");
    return r;
}

/* ditto for popen() */
static FILE*
popen_in_dir(const char *dir, const char *cmd, const char *type)
{
    char olddir[BUFSIZ];
    FILE *f;

    /* ensure that $SHELL exists, as the cmd was written relying on
       a non-blank $SHELL... */
    setenv("SHELL","/bin/sh",0); /* don't overwrite */

    if (getcwd(olddir, sizeof(olddir)) == 0)
	adios("getcwd", "could not get working directory");
    if (chdir(dir) != 0)
	adios("chdir", "could not change working directory");
    f = popen(cmd, type);
    if (chdir(olddir) != 0)
	adios("chdir", "could not change working directory");
    return f;
}


/*
 * EDIT
 */

static int  reedit = 0;		/* have we been here before?     */
static char *edsave = NULL;	/* the editor we used previously */


static int
editfile (char **ed, char **arg, char *file, int use, struct msgs *mp,
	  char *altmsg, char *cwd, int save_editor, int atfile)
{
    int pid, status, vecp;
    char altpath[BUFSIZ], linkpath[BUFSIZ];
    char *cp, *prog, **vec;
    struct stat st;

    int	slinked = 0;

    /* Was there a previous edit session? */
    if (reedit && (*ed || edsave)) {
	if (!*ed) {		/* no explicit editor      */
	    *ed = edsave;	/* so use the previous one */
	    if ((cp = r1bindex (*ed, '/')) == NULL)
		cp = *ed;

	    /* unless we've specified it via "editor-next" */
	    cp = concat (cp, "-next", NULL);
	    if ((cp = context_find (cp)) != NULL)
		*ed = cp;
	}
    } else {
	/* set initial editor */
	if (*ed == NULL)
	    *ed = get_default_editor();
    }

    if (altmsg) {
	if (mp == NULL || *altmsg == '/' || cwd == NULL)
	    strncpy (altpath, altmsg, sizeof(altpath));
	else
	    snprintf (altpath, sizeof(altpath), "%s/%s", mp->foldpath, altmsg);
	if (cwd == NULL)
	    strncpy (linkpath, LINK, sizeof(linkpath));
	else
	    snprintf (linkpath, sizeof(linkpath), "%s/%s", cwd, LINK);

	if (atfile) {
	    (void) m_unlink (linkpath);
	    if (link (altpath, linkpath) == NOTOK) {
		if (symlink (altpath, linkpath) < 0) {
		    adios (linkpath, "symlink");
		}
		slinked = 1;
	    } else {
		slinked = 0;
	    }
	}
    }

    context_save ();	/* save the context file */
    fflush (stdout);

    switch (pid = fork()) {
	case NOTOK:
	    advise ("fork", "unable to");
	    status = NOTOK;
	    break;

	case OK:
	    if (cwd) {
		if (chdir (cwd) < 0) {
		    advise (cwd, "chdir");
		}
	    }
	    if (altmsg) {
		if (mp)
		    m_putenv ("mhfolder", mp->foldpath);
		m_putenv ("editalt", altpath);
	    }

	    vec = argsplit(*ed, &prog, &vecp);

	    if (arg)
		while (*arg)
		    vec[vecp++] = *arg++;
	    vec[vecp++] = file;
	    vec[vecp] = NULL;

	    execvp (prog, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (*ed);
	    _exit (-1);

	default:
	    if ((status = pidwait (pid, NOTOK))) {
		if (((status & 0xff00) != 0xff00)
		    && (!reedit || (status & 0x00ff))) {
		    if (!use && (status & 0xff00) &&
			    (rename (file, cp = m_backup (file)) != NOTOK)) {
			advise (NULL, "problems with edit--draft left in %s", cp);
		    } else {
			advise (NULL, "problems with edit--%s preserved", file);
		    }
		}
		status = -2;	/* maybe "reedit ? -2 : -1"? */
		break;
	    }

	    reedit++;
	    if (altmsg
		    && mp
		    && !is_readonly(mp)
		    && (slinked
		           ? lstat (linkpath, &st) != NOTOK
				&& S_ISREG(st.st_mode)
				&& copyf (linkpath, altpath) == NOTOK
		           : stat (linkpath, &st) != NOTOK
				&& st.st_nlink == 1
				&& (m_unlink (altpath) == NOTOK
					|| link (linkpath, altpath) == NOTOK)))
		advise (linkpath, "unable to update %s from", altmsg);
    }

    /* normally, we remember which editor we used */
    if (save_editor)
	edsave = getcpy (*ed);

    *ed = NULL;
    if (altmsg && atfile)
	(void) m_unlink (linkpath);

    return status;
}


static int
copyf (char *ifile, char *ofile)
{
    int i, in, out;
    char buffer[BUFSIZ];

    if ((in = open (ifile, O_RDONLY)) == NOTOK)
	return NOTOK;
    if ((out = open (ofile, O_WRONLY | O_TRUNC)) == NOTOK) {
	admonish (ofile, "unable to open and truncate");
	close (in);
	return NOTOK;
    }

    while ((i = read (in, buffer, sizeof(buffer))) > OK)
	if (write (out, buffer, i) != i) {
	    advise (ofile, "may have damaged");
	    i = NOTOK;
	    break;
	}

    close (in);
    close (out);
    return i;
}


/*
 * SEND
 */

static int
sendfile (char **arg, char *file, int pushsw)
{
    pid_t child_id;
    int i, vecp;
    char *cp, *sp, **vec, *program;

    /*
     * If the sendproc is the nmh command `send', then we call
     * those routines directly rather than exec'ing the command.
     */
    if (strcmp (sp = r1bindex (sendproc, '/'), "send") == 0) {
	cp = invo_name;
	sendit (invo_name = sp, arg, file, pushsw);
	invo_name = cp;
	return 1;
    }

    context_save ();	/* save the context file */
    fflush (stdout);

    for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
	sleep (5);
    switch (child_id) {
	case NOTOK:
	    advise (NULL, "unable to fork, so sending directly...");
	case OK:
	    vec = argsplit(sendproc, &program, &vecp);
	    if (pushsw)
		vec[vecp++] = "-push";
	    if (arg)
		while (*arg)
		    vec[vecp++] = *arg++;
	    vec[vecp++] = file;
	    vec[vecp] = NULL;

	    execvp (program, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (sendproc);
	    _exit (-1);

	default:
	    if (pidwait(child_id, OK) == 0)
		done (0);
	    return 1;
    }
}


/*
 * Translate MIME composition file (call buildmimeproc)
 */

static int
buildfile (char **argp, char *file)
{
    int i;
    char **args, *ed;

    ed = buildmimeproc;

    /* allocate space for arguments */
    i = 0;
    if (argp) {
	while (argp[i])
	    i++;
    }
    args = (char **) mh_xmalloc((i + 2) * sizeof(char *));

    /*
     * For backward compatibility, we need to add -build
     * if we are using mhn as buildmimeproc
     */
    i = 0;
    if (strcmp (r1bindex (ed, '/'), "mhn") == 0)
	args[i++] = "-build";

    /* copy any other arguments */
    while (argp && *argp)
	args[i++] = *argp++;
    args[i] = NULL;

    i = editfile (&ed, args, file, NOUSE, NULL, NULL, NULL, 0, 0);
    free (args);

    return (i ? NOTOK : OK);
}


#ifndef CYRUS_SASL
# define SASLminc(a) (a)
#else /* CYRUS_SASL */
# define SASLminc(a)  0
#endif /* CYRUS_SASL */

#ifndef TLS_SUPPORT
# define TLSminc(a)  (a)
#else /* TLS_SUPPORT */
# define TLSminc(a)   0
#endif /* TLS_SUPPORT */

#define SEND_SWITCHES \
    X("alias aliasfile", 0, ALIASW) \
    X("debug", -5, DEBUGSW) \
    X("filter filterfile", 0, FILTSW) \
    X("nofilter", 0, NFILTSW) \
    X("format", 0, FRMTSW) \
    X("noformat", 0, NFRMTSW) \
    X("forward", 0, FORWSW) \
    X("noforward", 0, NFORWSW) \
    X("mime", 0, MIMESW) \
    X("nomime", 0, NMIMESW) \
    X("msgid", 0, MSGDSW) \
    X("nomsgid", 0, NMSGDSW) \
    X("push", 0, SPSHSW) \
    X("nopush", 0, NSPSHSW) \
    X("split seconds", 0, SPLITSW) \
    X("unique", -6, UNIQSW) \
    X("nounique", -8, NUNIQSW) \
    X("verbose", 0, VERBSW) \
    X("noverbose", 0, NVERBSW) \
    X("watch", 0, WATCSW) \
    X("nowatch", 0, NWATCSW) \
    X("width columns", 0, WIDTHSW) \
    X("version", 0, SVERSIONSW) \
    X("help", 0, SHELPSW) \
    X("dashstuffing", -12, BITSTUFFSW) \
    X("nodashstuffing", -14, NBITSTUFFSW) \
    X("client host", -6, CLIESW) \
    X("server host", 6, SERVSW) \
    X("snoop", -5, SNOOPSW) \
    X("draftfolder +folder", 0, SDRFSW) \
    X("draftmessage msg", 0, SDRMSW) \
    X("nodraftfolder", 0, SNDRFSW) \
    X("sasl", SASLminc(4), SASLSW) \
    X("nosasl", SASLminc(6), NOSASLSW) \
    X("saslmech", SASLminc(5), SASLMECHSW) \
    X("authservice", SASLminc(0), AUTHSERVICESW) \
    X("user username", SASLminc(4), USERSW) \
    X("attach fieldname", 6, SNDATTACHSW) \
    X("noattach", 0, SNDNOATTACHSW) \
    X("attachformat", 7, SNDATTACHFORMAT) \
    X("port server-port-name/number", 4, PORTSW) \
    X("tls", TLSminc(-3), TLSSW) \
    X("initialtls", TLSminc(-10), INITTLSSW) \
    X("notls", TLSminc(-5), NTLSSW) \
    X("sendmail program", 0, MTSSM) \
    X("mts smtp|sendmail/smtp|sendmail/pipe", 2, MTSSW) \
    X("messageid localname|random", 2, MESSAGEIDSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(SEND);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(SEND, sendswitches);
#undef X


extern int debugsw;		/* from sendsbr.c */
extern int forwsw;
extern int inplace;
extern int pushsw;
extern int splitsw;
extern int unique;
extern int verbsw;

extern char *altmsg;		/*  .. */
extern char *annotext;
extern char *distfile;


static void
sendit (char *sp, char **arg, char *file, int pushed)
{
    int	vecp, n = 1;
    char *cp, buf[BUFSIZ], **argp, *program;
    char **arguments, *savearg[MAXARGS], **vec;
    const char *user = NULL, *saslmech = NULL;
    char *auth_svc = NULL;
    int snoop = 0;
    struct stat st;

#ifndef	lint
    int	distsw = 0;
#endif

    /*
     * Make sure these are defined.  In particular, we need
     * savearg[1] to be NULL, in case "arg" is NULL below.  It
     * doesn't matter what is the value of savearg[0], but we
     * set it to NULL, to help catch "off-by-one" errors.
     */
    savearg[0] = NULL;
    savearg[1] = NULL;

    /*
     * Temporarily copy arg to savearg, since the brkstring() call in
     * getarguments() will wipe it out before it is merged in.
     * Also, we skip the first element of savearg, since getarguments()
     * skips it.  Then we count the number of arguments
     * copied.  The value of "n" will be one greater than
     * this in order to simulate the standard argc/argv.
     */
    if (arg) {
	char **bp;

	copyip (arg, savearg+1, MAXARGS-1);
	bp = savearg+1;
	while (*bp++)
	    n++;
    }

    /*
     * Merge any arguments from command line (now in savearg)
     * and arguments from profile.
     */
    arguments = getarguments (sp, n, savearg, 1);
    argp = arguments;

    debugsw = 0;
    forwsw = 1;
    inplace = 1;
    unique = 0;

    altmsg = NULL;
    annotext = NULL;
    distfile = NULL;

    /*
     * Get our initial arguments for postproc now
     */

    vec = argsplit(postproc, &program, &vecp);

    vec[vecp++] = "-library";
    vec[vecp++] = getcpy (m_maildir (""));

    if ((cp = context_find ("fileproc"))) {
	vec[vecp++] = "-fileproc";
	vec[vecp++] = cp;
    }

    if ((cp = context_find ("mhlproc"))) {
	vec[vecp++] = "-mhlproc";
	vec[vecp++] = cp;
    }

    if ((cp = context_find ("credentials"))) {
	/* post doesn't read context so need to pass credentials. */
	vec[vecp++] = "-credentials";
	vec[vecp++] = cp;
    }

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, sendswitches)) {
		case AMBIGSW:
		    ambigsw (cp, sendswitches);
		    return;
		case UNKWNSW:
		    advise (NULL, "-%s unknown\n", cp);
		    return;

		case SHELPSW:
		    snprintf (buf, sizeof(buf), "%s [switches]", sp);
		    print_help (buf, sendswitches, 1);
		    return;
		case SVERSIONSW:
		    print_version (invo_name);
		    return;

		case SPSHSW:
		    pushed++;
		    continue;
		case NSPSHSW:
		    pushed = 0;
		    continue;

		case SPLITSW:
		    if (!(cp = *argp++) || sscanf (cp, "%d", &splitsw) != 1) {
			advise (NULL, "missing argument to %s", argp[-2]);
			return;
		    }
		    continue;

		case UNIQSW:
		    unique++;
		    continue;
		case NUNIQSW:
		    unique = 0;
		    continue;
		case FORWSW:
		    forwsw++;
		    continue;
		case NFORWSW:
		    forwsw = 0;
		    continue;

		case VERBSW:
		    verbsw++;
		    vec[vecp++] = --cp;
		    continue;
		case NVERBSW:
		    verbsw = 0;
		    vec[vecp++] = --cp;
		    continue;

		case DEBUGSW:
		    debugsw++;	/* fall */
		case NFILTSW:
		case FRMTSW:
		case NFRMTSW:
		case BITSTUFFSW:
		case NBITSTUFFSW:
		case MIMESW:
		case NMIMESW:
		case MSGDSW:
		case NMSGDSW:
		case WATCSW:
		case NWATCSW:
		case SASLSW:
		case NOSASLSW:
		case TLSSW:
		case INITTLSSW:
		case NTLSSW:
		    vec[vecp++] = --cp;
		    continue;

		case SNOOPSW:
                    snoop++;
		    vec[vecp++] = --cp;
		    continue;

		case AUTHSERVICESW:
#ifdef OAUTH_SUPPORT
		    if (!(auth_svc = *argp++) || *auth_svc == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
#else
                    NMH_UNUSED (user);
                    NMH_UNUSED (auth_svc);
		    adios (NULL, "not built with OAuth support");
#endif
		    continue;

		case SASLMECHSW:
                    saslmech = *argp;
		    /* fall thru */
		case ALIASW:
		case FILTSW:
		case WIDTHSW:
		case CLIESW:
		case SERVSW:
		case USERSW:
		case PORTSW:
		case MTSSM:
		case MTSSW:
		case MESSAGEIDSW:
		    vec[vecp++] = --cp;
		    if (!(cp = *argp++) || *cp == '-') {
			advise (NULL, "missing argument to %s", argp[-2]);
			return;
		    }
		    vec[vecp++] = cp;
                    user = cp;
		    continue;

		case SDRFSW:
		case SDRMSW:
		    if (!(cp = *argp++) || *cp == '-') {
			advise (NULL, "missing argument to %s", argp[-2]);
			return;
		    }
		case SNDRFSW:
		    continue;

		case SNDATTACHSW:
		    advise(NULL, "The -attach switch is deprecated");
		    continue;
		case SNDNOATTACHSW:
		    advise(NULL, "The -noattach switch is deprecated");
		    continue;

		case SNDATTACHFORMAT:
		    advise(NULL, "The -attachformat switch is deprecated");
		    continue;
	    }
	}
	advise (NULL, "usage: %s [switches]", sp);
	return;
    }

    /* allow Aliasfile: profile entry */
    if ((cp = context_find ("Aliasfile"))) {
	char **ap, *dp;

	dp = mh_xstrdup(cp);
	for (ap = brkstring (dp, " ", "\n"); ap && *ap; ap++) {
	    vec[vecp++] = "-alias";
	    vec[vecp++] = *ap;
	}
    }

    if ((cp = getenv ("SIGNATURE")) == NULL || *cp == 0)
	if ((cp = context_find ("signature")) && *cp)
	    m_putenv ("SIGNATURE", cp);

    if ((annotext = getenv ("mhannotate")) == NULL || *annotext == 0)
	annotext = NULL;
    if ((altmsg = getenv ("mhaltmsg")) == NULL || *altmsg == 0)
	altmsg = NULL;
    if (annotext && ((cp = getenv ("mhinplace")) != NULL && *cp != 0))
	inplace = atoi (cp);

    if ((cp = getenv ("mhdist"))
	    && *cp
#ifndef lint
	    && (distsw = atoi (cp))
#endif /* not lint */
	    && altmsg) {
	vec[vecp++] = "-dist";
	if ((cp = m_mktemp2(altmsg, invo_name, NULL, NULL)) == NULL) {
	    adios(NULL, "unable to create temporary file in %s",
		  get_temp_dir());
	}
	distfile = mh_xstrdup(cp);
	(void) m_unlink(distfile);
	if (link (altmsg, distfile) == NOTOK)
	    adios (distfile, "unable to link %s to", altmsg);
    } else {
	distfile = NULL;
    }

#ifdef OAUTH_SUPPORT
    if (auth_svc == NULL) {
        if (saslmech  &&  ! strcasecmp(saslmech, "xoauth2")) {
            adios (NULL, "must specify -authservice with -saslmech xoauth2");
        }
    } else {
        if (user == NULL) {
            adios (NULL, "must specify -user with -saslmech xoauth2");
        }
    }
#else
    NMH_UNUSED(saslmech);
#endif /* OAUTH_SUPPORT */

    if (altmsg == NULL || stat (altmsg, &st) == NOTOK) {
	st.st_mtime = 0;
	st.st_dev = 0;
	st.st_ino = 0;
    }
    if ((pushsw = pushed))
	push ();

    closefds (3);

    if (sendsbr (vec, vecp, program, file, &st, 1, auth_svc) == OK)
	done (0);
}

/*
 * WHOM
 */

static int
whomfile (char **arg, char *file)
{
    pid_t pid;
    int vecp;
    char **vec, *program;

    context_save ();	/* save the context file */
    fflush (stdout);

    switch (pid = fork()) {
	case NOTOK:
	    advise ("fork", "unable to");
	    return 1;

	case OK:
	    vec = argsplit(whomproc, &program, &vecp);
	    if (arg)
		while (*arg)
		    vec[vecp++] = *arg++;
	    vec[vecp++] = file;
	    vec[vecp] = NULL;

	    execvp (program, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (whomproc);
	    _exit (-1);		/* NOTREACHED */

	default:
	    return (pidwait (pid, NOTOK) & 0377 ? 1 : 0);
    }
}


/*
 * Remove the draft file
 */

static int
removefile (char *drft)
{
    if (m_unlink (drft) == NOTOK)
	adios (drft, "unable to unlink");

    return OK;
}


/*
 * Return 1 if we already have a MIME-Version header, 0 otherwise.
 */

static int
checkmimeheader (char *drft)
{
    FILE *f;
    m_getfld_state_t gstate = 0;
    char buf[BUFSIZ], name[NAMESZ];
    int state, retval = 0;

    if ((f = fopen(drft, "r")) == NULL) {
	admonish(drft, "unable to read draft");
	return (0);
    }

    for (;;) {
	int bufsz = sizeof(buf);
	switch (state = m_getfld(&gstate, name, buf, &bufsz, f)) {
	case FLD:
	case FLDPLUS:
	    if (strcasecmp(name, VRSN_FIELD) == 0) {
		advise(NULL, "Cannot use attach commands with already-"
		       "formatted MIME message \"%s\"", drft);
		retval = 1;
		break;
	    }
	    continue;
	default:
	    break;
	}
	break;
    }

    m_getfld_state_destroy(&gstate);
    fclose(f);

    return retval;
}
