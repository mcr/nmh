
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
 *	attach files		This option attaches the named files to
 *				the draft.
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
#include <signal.h>
#include <h/mime.h>
#include <h/utils.h>

static struct swit whatnowswitches[] = {
#define	DFOLDSW                 0
    { "draftfolder +folder", 0 },
#define	DMSGSW                  1
    { "draftmessage msg", 0 },
#define	NDFLDSW                 2
    { "nodraftfolder", 0 },
#define	EDITRSW                 3
    { "editor editor", 0 },
#define	NEDITSW                 4
    { "noedit", 0 },
#define	PRMPTSW                 5
    { "prompt string", 4 },
#define VERSIONSW               6
    { "version", 0 },
#define	HELPSW                  7
    { "help", 0 },
#define	ATTACHSW                8
    { "attach header-field-name", 0 },
#define NOATTACHSW              9
    { "noattach", 0 },
    { NULL, 0 }
};

/*
 * Options at the "whatnow" prompt
 */
static struct swit aleqs[] = {
#define	EDITSW                         0
    { "edit [<editor> <switches>]", 0 },
#define	REFILEOPT                      1
    { "refile [<switches>] +folder", 0 },
#define BUILDMIMESW                    2
    { "mime [<switches>]", 0 },
#define	DISPSW                         3
    { "display [<switches>]", 0 },
#define	LISTSW                         4
    { "list [<switches>]", 0 },
#define	SENDSW                         5
    { "send [<switches>]", 0 },
#define	PUSHSW                         6
    { "push [<switches>]", 0 },
#define	WHOMSW                         7
    { "whom [<switches>]", 0 },
#define	QUITSW                         8
    { "quit [-delete]", 0 },
#define DELETESW                       9
    { "delete", 0 },
#define	CDCMDSW			      10
    { "cd [directory]", 0 },
#define	PWDCMDSW		      11
    { "pwd", 0 },
#define	LSCMDSW			      12
    { "ls", 2 },
#define	ATTACHCMDSW		      13
    { "attach", 0 },
#define	DETACHCMDSW		      14
    { "detach [-n]", 2 },
#define	ALISTCMDSW		      15
    { "alist [-ln] ", 2 },
    { NULL, 0 }
};

static char *myprompt = "\nWhat now? ";

/*
 * static prototypes
 */
static int editfile (char **, char **, char *, int, struct msgs *,
	char *, char *, int, int);
static int sendfile (char **, char *, int);
static void sendit (char *, char **, char *, int);
static int buildfile (char **, char *);
static int check_draft (char *);
static int whomfile (char **, char *);
static int removefile (char *);
static void writelscmd(char *, int, char *, char **);
static void writesomecmd(char *buf, int bufsz, char *cmd, char *trailcmd, char **argp);
static FILE* popen_in_dir(const char *dir, const char *cmd, const char *type);
static int system_in_dir(const char *dir, const char *cmd);


#ifdef HAVE_LSTAT
static int copyf (char *, char *);
#endif


int
WhatNow (int argc, char **argv)
{
    int isdf = 0, nedit = 0, use = 0, atfile = 1;
    char *cp, *dfolder = NULL, *dmsg = NULL;
    char *ed = NULL, *drft = NULL, *msgnam = NULL;
    char buf[BUFSIZ], prompt[BUFSIZ];
    char **argp, **arguments;
    struct stat st;
    char	*attach = NMH_ATTACH_HEADER;/* attachment header field name */
    char	cwd[PATH_MAX + 1];	/* current working directory */
    char	file[PATH_MAX + 1];	/* file name buffer */
    char	shell[PATH_MAX + 1];	/* shell response buffer */
    FILE	*f;			/* read pointer for bgnd proc */
    char	*l;			/* set on -l to alist  command */
    int		n;			/* set on -n to alist command */

    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /*
     *	Get the initial current working directory.
     */

    if (getcwd(cwd, sizeof (cwd)) == (char *)0) {
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
		if (!(attach = *argp++) || *attach == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;

	    case NOATTACHSW:
	    	attach = NULL;
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

    msgnam = (cp = getenv ("mhaltmsg")) && *cp ? getcpy (cp) : NULL;

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
	if (!(argp = getans_via_readline (prompt, aleqs))) {
#else /* ! READLINE_SUPPORT */
	if (!(argp = getans (prompt, aleqs))) {
#endif /* READLINE_SUPPORT */
	    unlink (LINK);
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

	    if (*(argp+1) == (char *)0) {
		(void)sprintf(buf, "$SHELL -c \"cd&&pwd\"");
	    }
	    else {
		writesomecmd(buf, BUFSIZ, "cd", "pwd", argp);
	    }
	    if ((f = popen_in_dir(cwd, buf, "r")) != (FILE *)0) {
		fgets(cwd, sizeof (cwd), f);

		if (strchr(cwd, '\n') != (char *)0)
			*strchr(cwd, '\n') = '\0';

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

	    if (attach == (char *)0) {
		advise((char *)0, "can't list because no header field name was given.");
		break;
	    }

	    l = (char *)0;
	    n = 0;

	    while (*++argp != (char *)0) {
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
		advise((char *)0, "usage is alist [-ln].");

	    else
		annolist(drft, attach, l, n);

	    break;

	case ATTACHCMDSW:
	    /*
	     *	Attach files to current draft.
	     */

	    if (attach == (char *)0) {
		advise((char *)0, "can't attach because no header field name was given.");
		break;
	    }

	    if (*(argp+1) == (char *)0) {
		advise((char *)0, "attach command requires file argument(s).");
		break;
	    }

	    /*
	     *	Build a command line that causes the user's shell to list the file name
	     *	arguments.  This handles and wildcard expansion, tilde expansion, etc.
	     */
	    writelscmd(buf, sizeof(buf), "-d", argp);

	    /*
	     *	Read back the response from the shell, which contains a number of lines
	     *	with one file name per line.  Remove off the newline.  Determine whether
	     *	we have an absolute or relative path name.  Prepend the current working
	     *	directory to relative path names.  Add the attachment annotation to the
	     *	draft.
	     */

	    if ((f = popen_in_dir(cwd, buf, "r")) != (FILE *)0) {
		while (fgets(shell, sizeof (shell), f) != (char *)0) {
		    *(strchr(shell, '\n')) = '\0';

		    if (*shell == '/')
			(void)annotate(drft, attach, shell, 1, 0, -2, 1);
		    else {
			(void)sprintf(file, "%s/%s", cwd, shell);
			(void)annotate(drft, attach, file, 1, 0, -2, 1);
		    }
		}

		pclose(f);
	    }
	    else {
		advise("popen", "could not get file from shell");
	    }

	    break;

	case DETACHCMDSW:
	    /*
	     *	Detach files from current draft.
	     */

	    if (attach == (char *)0) {
		advise((char *)0, "can't detach because no header field name was given.");
		break;
	    }

	    /*
	     *	Scan the arguments for a -n.  Mixed file names and numbers aren't allowed,
	     *	so this catches a -n anywhere in the argument list.
	     */

	    for (n = 0, arguments = argp + 1; *arguments != (char *)0; arguments++) {
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
		for (arguments = argp + 1; *arguments != (char *)0; arguments++) {
		    if (strcmp(*arguments, "-n") == 0)
			continue;

		    if (**arguments != '\0') {
			n = atoi(*arguments);
			(void)annotate(drft, attach, (char *)0, 1, 0, n, 1);

			for (argp = arguments + 1; *argp != (char *)0; argp++) {
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
	    writelscmd(buf, sizeof(buf), "-d", argp);
	    if ((f = popen_in_dir(cwd, buf, "r")) != (FILE *)0) {
		while (fgets(shell, sizeof (shell), f) != (char *)0) {
		    *(strchr(shell, '\n')) = '\0';
		    (void)annotate(drft, attach, shell, 1, 0, 0, 1);
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
	adios((char *)0, "arguments too long");
    
    cp = buf + ln;
    
    while (*++argp != (char *)0) {
	ln = strlen(*argp);
	/* +1 for leading space */
	if (ln + trailln + 1 > bufsz - (cp-buf))
	    adios((char *)0, "arguments too long");
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
  char *lscmd = concat ("ls ", lsoptions, " --", NULL);
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
    char *cp, *vec[MAXARGS];
    struct stat st;

#ifdef HAVE_LSTAT
    int	slinked = 0;
#endif /* HAVE_LSTAT */

    /* Was there a previous edit session? */
    if (reedit) {
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
	if (*ed == NULL && (*ed = context_find ("editor")) == NULL)
	    *ed = defaulteditor;
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
	    unlink (linkpath);
#ifdef HAVE_LSTAT
	    if (link (altpath, linkpath) == NOTOK) {
		symlink (altpath, linkpath);
		slinked = 1;
	    } else {
		slinked = 0;
	    }
#else /* not HAVE_LSTAT */
	    link (altpath, linkpath);
#endif /* not HAVE_LSTAT */
	}
    }

    context_save ();	/* save the context file */
    fflush (stdout);

    switch (pid = vfork()) {
	case NOTOK:
	    advise ("fork", "unable to");
	    status = NOTOK;
	    break;

	case OK:
	    if (cwd)
		chdir (cwd);
	    if (altmsg) {
		if (mp)
		    m_putenv ("mhfolder", mp->foldpath);
		m_putenv ("editalt", altpath);
	    }

	    vecp = 0;
	    vec[vecp++] = r1bindex (*ed, '/');
	    if (arg)
		while (*arg)
		    vec[vecp++] = *arg++;
	    vec[vecp++] = file;
	    vec[vecp] = NULL;

	    execvp (*ed, vec);
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
#ifdef HAVE_LSTAT
	    if (altmsg
		    && mp
		    && !is_readonly(mp)
		    && (slinked
		           ? lstat (linkpath, &st) != NOTOK
				&& S_ISREG(st.st_mode)
				&& copyf (linkpath, altpath) == NOTOK
		           : stat (linkpath, &st) != NOTOK
				&& st.st_nlink == 1
				&& (unlink (altpath) == NOTOK
					|| link (linkpath, altpath) == NOTOK)))
		advise (linkpath, "unable to update %s from", altmsg);
#else /* HAVE_LSTAT */
	    if (altmsg
		    && mp
		    && !is_readonly(mp)
		    && stat (linkpath, &st) != NOTOK
		    && st.st_nlink == 1
		    && (unlink (altpath) == NOTOK
			|| link (linkpath, altpath) == NOTOK))
		advise (linkpath, "unable to update %s from", altmsg);
#endif /* HAVE_LSTAT */
    }

    /* normally, we remember which editor we used */
    if (save_editor)
	edsave = getcpy (*ed);

    *ed = NULL;
    if (altmsg && atfile)
	unlink (linkpath);

    return status;
}


#ifdef HAVE_LSTAT
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
#endif /* HAVE_LSTAT */


/*
 * SEND
 */

static int
sendfile (char **arg, char *file, int pushsw)
{
    pid_t child_id;
    int i, vecp;
    char *cp, *sp, *vec[MAXARGS];

    /* Translate MIME composition file, if necessary */
    if ((cp = context_find ("automimeproc"))
	    && (!strcmp (cp, "1"))
	    && check_draft (file)
	    && (buildfile (NULL, file) == NOTOK))
	return 0;

    /* For backwards compatibility */
    if ((cp = context_find ("automhnproc"))
	    && check_draft (file)
	    && (i = editfile (&cp, NULL, file, NOUSE, NULL, NULL, NULL, 0, 0)))
	return 0;

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

    for (i = 0; (child_id = vfork()) == NOTOK && i < 5; i++)
	sleep (5);
    switch (child_id) {
	case NOTOK:
	    advise (NULL, "unable to fork, so sending directly...");
	case OK:
	    vecp = 0;
	    vec[vecp++] = invo_name;
	    if (pushsw)
		vec[vecp++] = "-push";
	    if (arg)
		while (*arg)
		    vec[vecp++] = *arg++;
	    vec[vecp++] = file;
	    vec[vecp] = NULL;

	    execvp (sendproc, vec);
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


/*
 *  Check if draft is a mhbuild composition file
 */

static int
check_draft (char *msgnam)
{
    int	state;
    char buf[BUFSIZ], name[NAMESZ];
    FILE *fp;

    if ((fp = fopen (msgnam, "r")) == NULL)
	return 0;
    for (state = FLD;;) {
	int bufsz = sizeof buf;
	switch (state = m_getfld (state, name, buf, &bufsz, fp)) {
	    case FLD:
	    case FLDPLUS:
		/*
		 * If draft already contains any of the
		 * Content-XXX fields, then assume it already
		 * been converted.
		 */
	        if (uprf (name, XXX_FIELD_PRF)) {
		    fclose (fp);
		    return 0;
		}
		while (state == FLDPLUS) {
		    bufsz = sizeof buf;
		    state = m_getfld (state, name, buf, &bufsz, fp);
		}
		break;

	    case BODY:
	        do {
		    char *bp;

		    for (bp = buf; *bp; bp++)
			if (*bp != ' ' && *bp != '\t' && *bp != '\n') {
			    fclose (fp);
			    return 1;
			}

		    bufsz = sizeof buf;
		    state = m_getfld (state, name, buf, &bufsz, fp);
		} while (state == BODY);
		/* and fall... */

	    default:
		fclose (fp);
		return 0;
	}
    }
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

static struct swit  sendswitches[] = {
#define	ALIASW            0
    { "alias aliasfile", 0 },
#define	DEBUGSW           1
    { "debug", -5 },
#define	FILTSW            2
    { "filter filterfile", 0 },
#define	NFILTSW           3
    { "nofilter", 0 },
#define	FRMTSW            4
    { "format", 0 },
#define	NFRMTSW           5
    { "noformat", 0 },
#define	FORWSW            6
    { "forward", 0 },
#define	NFORWSW           7
    { "noforward", 0 },
#define MIMESW            8
    { "mime", 0 },
#define NMIMESW           9
    { "nomime", 0 },
#define MSGDSW           10
    { "msgid", 0 },
#define NMSGDSW          11
    { "nomsgid", 0 },
#define SPSHSW           12
    { "push", 0 },
#define NSPSHSW          13
    { "nopush", 0 },
#define SPLITSW          14
    { "split seconds", 0 },
#define UNIQSW           15
    { "unique", -6 },
#define NUNIQSW          16
    { "nounique", -8 },
#define VERBSW           17
    { "verbose", 0 },
#define NVERBSW          18
    { "noverbose", 0 },
#define	WATCSW           19
    { "watch", 0 },
#define	NWATCSW          20
    { "nowatch", 0 },
#define	WIDTHSW          21
    { "width columns", 0 },
#define SVERSIONSW       22
    { "version", 0 },
#define	SHELPSW          23
    { "help", 0 },
#define BITSTUFFSW       24
    { "dashstuffing", -12 },
#define NBITSTUFFSW      25
    { "nodashstuffing", -14 },
#define	MAILSW           26
    { "mail", -4 },
#define	SAMLSW           27
    { "saml", -4 },
#define	SSNDSW           28
    { "send", -4 },
#define	SOMLSW           29
    { "soml", -4 },
#define	CLIESW           30
    { "client host", -6 },
#define	SERVSW           31
    { "server host", 6 },
#define	SNOOPSW          32
    { "snoop", -5 },
#define SDRFSW           33
    { "draftfolder +folder", -6 },
#define SDRMSW           34
    { "draftmessage msg", -6 },
#define SNDRFSW          35
    { "nodraftfolder", -3 },
#define SASLSW           36
    { "sasl", SASLminc(-4) },
#define NOSASLSW         37
    { "nosasl", SASLminc(-6) },
#define SASLMXSSFSW      38
    { "saslmaxssf", SASLminc(-10) },
#define SASLMECHSW       39
    { "saslmech", SASLminc(-5) },
#define USERSW           40
    { "user", SASLminc(-4) },
#define SNDATTACHSW       41
    { "attach file", 6 },
#define SNDNOATTACHSW     42
    { "noattach", 0 },
#define SNDATTACHFORMAT   43
    { "attachformat", 7 },
#define PORTSW		  44
    { "port server-port-name/number", 4 },
#define TLSSW		  45
    { "tls", TLSminc(-3) },
#define NTLSSW            46
    { "notls", TLSminc(-5) },
#define MTSSW		  47
    { "mts smtp|sendmail/smtp|sendmail/pipe", 2 },
#define MESSAGEIDSW	  48
    { "messageid localname|random", 2 },
    { NULL, 0 }
};


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
    char *cp, buf[BUFSIZ], **argp;
    char **arguments, *vec[MAXARGS];
    struct stat st;
    char	*attach = NMH_ATTACH_HEADER;/* attachment header field name */
    int		attachformat = 1;	/* mhbuild format specifier for
					   attachments */

#ifndef	lint
    int	distsw = 0;
#endif

    /*
     * Make sure these are defined.  In particular, we need
     * vec[1] to be NULL, in case "arg" is NULL below.  It
     * doesn't matter what is the value of vec[0], but we
     * set it to NULL, to help catch "off-by-one" errors.
     */
    vec[0] = NULL;
    vec[1] = NULL;

    /*
     * Temporarily copy arg to vec, since the brkstring() call in
     * getarguments() will wipe it out before it is merged in.
     * Also, we skip the first element of vec, since getarguments()
     * skips it.  Then we count the number of arguments
     * copied.  The value of "n" will be one greater than
     * this in order to simulate the standard argc/argv.
     */
    if (arg) {
	char **bp;

	copyip (arg, vec+1, MAXARGS-1);
	bp = vec+1;
	while (*bp++)
	    n++;
    }

    /*
     * Merge any arguments from command line (now in vec)
     * and arguments from profile.
     */
    arguments = getarguments (sp, n, vec, 1);
    argp = arguments;

    debugsw = 0;
    forwsw = 1;
    inplace = 1;
    unique = 0;

    altmsg = NULL;
    annotext = NULL;
    distfile = NULL;

    vecp = 1;			/* we'll get the zero'th element later */
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
		case MAILSW:
		case SAMLSW:
		case SSNDSW:
		case SOMLSW:
		case SNOOPSW:
		case SASLSW:
		case NOSASLSW:
		case TLSSW:
		case NTLSSW:
		    vec[vecp++] = --cp;
		    continue;

		case ALIASW:
		case FILTSW:
		case WIDTHSW:
		case CLIESW:
		case SERVSW:
		case SASLMXSSFSW:
		case SASLMECHSW:
		case USERSW:
		case PORTSW:
		case MTSSW:
		case MESSAGEIDSW:
		    vec[vecp++] = --cp;
		    if (!(cp = *argp++) || *cp == '-') {
			advise (NULL, "missing argument to %s", argp[-2]);
			return;
		    }
		    vec[vecp++] = cp;
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
		    if (!(attach = *argp++) || *attach == '-') {
			advise (NULL, "missing argument to %s", argp[-2]);
			return;
		    }
		    continue;
		case SNDNOATTACHSW:
		    attach = NULL;
		    continue;

		case SNDATTACHFORMAT:
		    if (! *argp || **argp == '-')
			adios (NULL, "missing argument to %s", argp[-1]);
		    else {
			attachformat = atoi (*argp);
			if (attachformat < 0 ||
			    attachformat > ATTACHFORMATS - 1) {
			    advise (NULL, "unsupported attachformat %d",
				    attachformat);
			    continue;
			}
		    }
		    ++argp;
		    continue;
	    }
	}
	advise (NULL, "usage: %s [switches]", sp);
	return;
    }

    /* allow Aliasfile: profile entry */
    if ((cp = context_find ("Aliasfile"))) {
	char **ap, *dp;

	dp = getcpy (cp);
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
	distfile = getcpy (m_mktemp2(altmsg, invo_name, NULL, NULL));
	unlink(distfile);
	if (link (altmsg, distfile) == NOTOK)
	    adios (distfile, "unable to link %s to", altmsg);
    } else {
	distfile = NULL;
    }

    if (altmsg == NULL || stat (altmsg, &st) == NOTOK) {
	st.st_mtime = 0;
	st.st_dev = 0;
	st.st_ino = 0;
    }
    if ((pushsw = pushed))
	push ();

    vec[0] = r1bindex (postproc, '/');
    closefds (3);

    if (sendsbr (vec, vecp, file, &st, 1, attach, attachformat) == OK)
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
    char *vec[MAXARGS];

    context_save ();	/* save the context file */
    fflush (stdout);

    switch (pid = vfork()) {
	case NOTOK:
	    advise ("fork", "unable to");
	    return 1;

	case OK:
	    vecp = 0;
	    vec[vecp++] = r1bindex (whomproc, '/');
	    vec[vecp++] = file;
	    if (arg)
		while (*arg)
		    vec[vecp++] = *arg++;
	    vec[vecp] = NULL;

	    execvp (whomproc, vec);
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
    if (unlink (drft) == NOTOK)
	adios (drft, "unable to unlink");

    return OK;
}
