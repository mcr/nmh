
/*
 * dist.c -- re-distribute a message
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <fcntl.h>

static struct swit switches[] = {
#define	ANNOSW	0
    { "annotate", 0 },
#define	NANNOSW	1
    { "noannotate", 0 },
#define	DFOLDSW	2
    { "draftfolder +folder", 0 },
#define	DMSGSW	3
    { "draftmessage msg", 0 },
#define	NDFLDSW	4
    { "nodraftfolder", 0 },
#define	EDITRSW	5
    { "editor editor", 0 },
#define	NEDITSW	6
    { "noedit", 0 },
#define	FORMSW	7
    { "form formfile", 0 },
#define	INPLSW	8
    { "inplace", 0 },
#define	NINPLSW	9
    { "noinplace", 0 },
#define	WHATSW	10
    { "whatnowproc program", 0 },
#define	NWHATSW	11
    { "nowhatnowproc", 0 },
#define VERSIONSW 12
    { "version", 0 },
#define	HELPSW	13
    { "help", 0 },
#define	FILESW	14
    { "file file", -4 },	/* interface from msh */
    { NULL, 0 }
};

static struct swit aqrnl[] = {
#define	NOSW	0
    { "quit", 0 },
#define	YESW	1
    { "replace", 0 },
#define	LISTDSW	2
    { "list", 0 },
#define	REFILSW	3
    { "refile +folder", 0 },
#define NEWSW	4
    { "new", 0 },
    { NULL, 0 }
};


static struct swit aqrl[] = {
    { "quit", 0 },
    { "replace", 0 },
    { "list", 0 },
    { "refile +folder", 0 },
    { NULL, 0 }
};


int
main (int argc, char **argv)
{
    int anot = 0, inplace = 1, nedit = 0;
    int nwhat = 0, i, in, isdf = 0, out;
    char *cp, *cwd, *maildir, *msgnam, *dfolder = NULL;
    char *dmsg = NULL, *ed = NULL, *file = NULL, *folder = NULL;
    char *form = NULL, *msg = NULL, buf[BUFSIZ], drft[BUFSIZ];
    char **argp, **arguments;
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

		case ANNOSW: 
		    anot++;
		    continue;
		case NANNOSW: 
		    anot = 0;
		    continue;

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

		case FILESW: 
		    if (file)
			adios (NULL, "only one file at a time!");
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    file = path (cp, TFILE);
		    continue;
		case FORMSW: 
		    if (!(form = *argp++) || *form == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case INPLSW: 
		    inplace++;
		    continue;
		case NINPLSW: 
		    inplace = 0;
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
		    if (dmsg)
			adios (NULL, "only one draft message at a time!");
		    if (!(dmsg = *argp++) || *dmsg == '-')
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
		folder = pluspath (cp);
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
    if (file && (msg || folder))
	adios (NULL, "can't mix files and folders/msgs");

    in = open_form(&form, distcomps);

try_it_again:
    strncpy (drft, m_draft (dfolder, dmsg, NOUSE, &isdf), sizeof(drft));

    /* Check if draft already exists */
    if (stat (drft, &st) != NOTOK) {
	printf ("Draft \"%s\" exists (%ld bytes).", drft, (long) st.st_size);
	for (i = LISTDSW; i != YESW;) {
	    if (!(argp = getans ("\nDisposition? ", isdf ? aqrnl : aqrl)))
		done (1);
	    switch (i = smatch (*argp, isdf ? aqrnl : aqrl)) {
		case NOSW: 
		    done (0);
		case NEWSW: 
		    dmsg = NULL;
		    goto try_it_again;
		case YESW: 
		    break;
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
    }
    if ((out = creat (drft, m_gmprot ())) == NOTOK)
	adios (drft, "unable to create");

    cpydata (in, out, form, drft);
    close (in);
    close (out);

    if (file) {
	/*
	 * Dist a file
	 */
	anot = 0;	/* don't want to annotate a file */
    } else {
	/*
	 * Dist a message
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
    }

    msgnam = file ? file : getcpy (m_name (mp->lowsel));
    if ((in = open (msgnam, O_RDONLY)) == NOTOK)
	adios (msgnam, "unable to open message");

    if (!file) {
	context_replace (pfolder, folder);/* update current folder  */
	seq_setcur (mp, mp->lowsel);	  /* update current message */
	seq_save (mp);			  /* synchronize sequences  */
	context_save ();		  /* save the context file  */
    }

    if (nwhat)
	done (0);
    what_now (ed, nedit, NOUSE, drft, msgnam, 1, mp,
	anot ? "Resent" : NULL, inplace, cwd);
    done (1);
    return 1;
}
