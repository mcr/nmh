
/*
 * comp.c -- compose a message
 *
 * $Id$
 */

#include <h/mh.h>
#include <fcntl.h>

static struct swit switches[] = {
#define	DFOLDSW                0
    { "draftfolder +folder", 0 },
#define	DMSGSW                 1
    { "draftmessage msg", 0 },
#define	NDFLDSW                2
    { "nodraftfolder", 0 },
#define	EDITRSW                3
    { "editor editor", 0 },
#define	NEDITSW                4
    { "noedit", 0 },
#define	FILESW                 5
    { "file file", 0 },
#define	FORMSW                 6
    { "form formfile", 0 },
#define	USESW                  7
    { "use", 0 },
#define	NUSESW                 8
    { "nouse", 0 },
#define	WHATSW                 9
    { "whatnowproc program", 0 },
#define	NWHATSW               10
    { "nowhatnowproc", 0 },
#define VERSIONSW             11
    { "version", 0 },
#define	HELPSW                12
    { "help", 4 },
    { NULL, 0 }
};

static struct swit aqrunl[] = {
#define	NOSW          0
    { "quit", 0 },
#define	YESW          1
    { "replace", 0 },
#define	USELSW        2
    { "use", 0 },
#define	LISTDSW       3
    { "list", 0 },
#define	REFILSW       4
    { "refile +folder", 0 },
#define NEWSW         5
    { "new", 0 },
    { NULL, 0 }
};

static struct swit aqrul[] = {
    { "quit", 0 },
    { "replace", 0 },
    { "use", 0 },
    { "list", 0 },
    { "refile", 0 },
    { NULL, 0 }
};


int
main (int argc, char **argv)
{
    int use = NOUSE, nedit = 0, nwhat = 0;
    int i, in, isdf = 0, out;
    char *cp, *cwd, *maildir, *dfolder = NULL;
    char *ed = NULL, *file = NULL, *form = NULL;
    char *folder = NULL, *msg = NULL, buf[BUFSIZ];
    char drft[BUFSIZ], **argp, **arguments;
    struct msgs *mp = NULL;
    struct stat st;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [+folder] [msg] [switches]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case EDITRSW: 
		    if (!(ed = *argp++) || *ed == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    nedit = 0;
		    continue;
		case NEDITSW: 
		    nedit++;
		    continue;

		case WHATSW: 
		    if (!(whatnowproc = *argp++) || *whatnowproc == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    nwhat = 0;
		    continue;
		case NWHATSW: 
		    nwhat++;
		    continue;

		case FORMSW: 
		    if (!(form = *argp++) || *form == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case USESW: 
		    use++;
		    continue;
		case NUSESW: 
		    use = NOUSE;
		    continue;

		case FILESW: 	/* compatibility */
		    if (file)
			adios (NULL, "only one file at a time!");
		    if (!(file = *argp++) || *file == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    isdf = NOTOK;
		    continue;

		case DFOLDSW: 
		    if (dfolder)
			adios (NULL, "only one draft folder at a time!");
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    dfolder = path (*cp == '+' || *cp == '@' ? cp + 1 : cp,
			    *cp != '@' ? TFOLDER : TSUBCWF);
		    continue;
		case DMSGSW: 
		    if (file)
			adios (NULL, "only one draft message at a time!");
		    if (!(file = *argp++) || *file == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case NDFLDSW: 
		    dfolder = NULL;
		    isdf = NOTOK;
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		adios (NULL, "only one folder at a time!");
	    else
		folder = path (cp + 1, *cp == '+' ? TFOLDER : TSUBCWF);
	} else {
	    if (msg)
		adios (NULL, "only one message at a time!");
	    else
		msg = cp;
	}
    }

    cwd = getcpy (pwd ());

    if (!context_find ("path"))
	free (path ("./", TFOLDER));

    /*
     * Check if we are using a draft folder
     * and have specified a message in it.
     */
    if ((dfolder || context_find ("Draft-Folder")) && !folder && msg && !file) {
	file = msg;
	msg = NULL;
    }
    if (form && (folder || msg))
	    adios (NULL, "can't mix forms and folders/msgs");

    if (folder || msg) {
	/*
	 * Use a message as the "form" for the new message.
	 */
	if (!msg)
	    msg = "cur";
	if (!folder)
	    folder = getfolder (1);
	maildir = m_maildir (folder);

	if (chdir (maildir) == NOTOK)
	    adios (maildir, "unable to change directory to");

	/* read folder and create message structure */
	if (!(mp = folder_read (folder)))
	    adios (NULL, "unable to read folder %s", folder);

	/* check for empty folder */
	if (mp->nummsg == 0)
	    adios (NULL, "no messages in %s", folder);

	/* parse the message range/sequence/name and set SELECTED */
	if (!m_convert (mp, msg))
	    done (1);
	seq_setprev (mp);	/* set the previous-sequence */

	if (mp->numsel > 1)
	    adios (NULL, "only one message at a time!");

	if ((in = open (form = getcpy (m_name (mp->lowsel)), O_RDONLY)) == NOTOK)
	    adios (form, "unable to open message");
    } else {
	/*
	 * Open a component or forms file
	 */
	if (form) {
	    if ((in = open (etcpath (form), O_RDONLY)) == NOTOK)
		adios (form, "unable to open form file");
	} else {
	    if ((in = open (etcpath (components), O_RDONLY)) == NOTOK)
		adios (components, "unable to open default components file");
	    form = components;
	}
    }

try_it_again:
    strncpy (drft, m_draft (dfolder, file, use, &isdf), sizeof(drft));

    /*
     * Check if we have an existing draft
     */
    if ((out = open (drft, O_RDONLY)) != NOTOK) {
	i = fdcompare (in, out);
	close (out);

	/*
	 * If we have given -use flag, or if the
	 * draft is just the same as the components
	 * file, then no need to ask any questions.
	 */
	if (use || i)
	    goto edit_it;

	if (stat (drft, &st) == NOTOK)
	    adios (drft, "unable to stat");
	printf ("Draft \"%s\" exists (%ld bytes).", drft, (long) st.st_size);
	for (i = LISTDSW; i != YESW;) {
	    if (!(argp = getans ("\nDisposition? ", isdf ? aqrunl : aqrul)))
		done (1);
	    switch (i = smatch (*argp, isdf ? aqrunl : aqrul)) {
		case NOSW: 
		    done (0);
		case NEWSW: 
		    file = NULL;
		    use = NOUSE;
		    goto try_it_again;
		case YESW: 
		    break;
		case USELSW:
		    use++;
		    goto edit_it;
		case LISTDSW: 
		    showfile (++argp, drft);
		    break;
		case REFILSW: 
		    if (refile (++argp, drft) == 0)
			i = YESW;
		    break;
		default: 
		    advise (NULL, "say what?");
		    break;
	    }
	}
    } else {
	if (use)
	    adios (drft, "unable to open");
    }

    if ((out = creat (drft, m_gmprot ())) == NOTOK)
	adios (drft, "unable to create");
    cpydata (in, out, form, drft);
    close (in);
    close (out);

edit_it:
    context_save ();	/* save the context file */

    if (nwhat)
	done (0);
    what_now (ed, nedit, use, drft, NULL, 0, NULLMP, NULL, 0, cwd);
    return done (1);
}
