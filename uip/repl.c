
/*
 * repl.c -- reply to a message
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>


static struct swit switches[] = {
#define GROUPSW                0
    { "group", 0 },
#define NGROUPSW               1
    { "nogroup", 0 },
#define	ANNOSW                 2
    { "annotate", 0 },
#define	NANNOSW                3
    { "noannotate", 0 },
#define	CCSW                   4
    { "cc all|to|cc|me", 0 },
#define	NCCSW                  5
    { "nocc type", 0 },
#define	DFOLDSW                6
    { "draftfolder +folder", 0 },
#define	DMSGSW                 7
    { "draftmessage msg", 0 },
#define	NDFLDSW                8
    { "nodraftfolder", 0 },
#define	EDITRSW                9
    { "editor editor", 0 },
#define	NEDITSW               10
    { "noedit", 0 },
#define	FCCSW                 11
    { "fcc folder", 0 },
#define	FILTSW                12
    { "filter filterfile", 0 },
#define	FORMSW                13
    { "form formfile", 0 },
#define	FRMTSW                14
    { "format", 5 },
#define	NFRMTSW               15
    { "noformat", 7 },
#define	INPLSW                16
    { "inplace", 0 },
#define	NINPLSW               17
    { "noinplace", 0 },
#define MIMESW                18
    { "mime", 0 },
#define NMIMESW               19
    { "nomime", 0 },
#define	QURYSW                20
    { "query", 0 },
#define	NQURYSW               21
    { "noquery", 0 },
#define	WHATSW                22
    { "whatnowproc program", 0 },
#define	NWHATSW               23
    { "nowhatnowproc", 0 },
#define	WIDTHSW               24
    { "width columns", 0 },
#define VERSIONSW             25
    { "version", 0 },
#define	HELPSW                26
    { "help", 0 },
#define	FILESW                27
    { "file file", 4 },			/* interface from msh */

#ifdef MHE
#define	BILDSW                28
    { "build", 5 },			/* interface from mhe */
#endif

    { NULL, 0 }
};

static struct swit ccswitches[] = {
#define	CTOSW	0
    { "to", 0 },
#define	CCCSW	1
    { "cc", 0 },
#define	CMESW	2
    { "me", 0 },
#define	CALSW	3
    { "all", 0 },
    { NULL, 0 }
};

static struct swit aqrnl[] = {
#define	NOSW         0
    { "quit", 0 },
#define	YESW         1
    { "replace", 0 },
#define	LISTDSW      2
    { "list", 0 },
#define	REFILSW      3
    { "refile +folder", 0 },
#define NEWSW        4
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

short ccto = -1;		/* global for replsbr */
short cccc = -1;
short ccme = -1;
short querysw = 0;

short outputlinelen = OUTPUTLINELEN;
short groupreply = 0;		/* Is this a group reply?        */

int mime = 0;			/* include original as MIME part */
char *form   = NULL;		/* form (components) file        */
char *filter = NULL;		/* message filter file           */
char *fcc    = NULL;		/* folders to add to Fcc: header */


/*
 * prototypes
 */
void docc (char *, int);


int
main (int argc, char **argv)
{
    int	i, isdf = 0;
    int anot = 0, inplace = 1;
    int nedit = 0, nwhat = 0;
    char *cp, *cwd, *dp, *maildir, *file = NULL;
    char *folder = NULL, *msg = NULL, *dfolder = NULL;
    char *dmsg = NULL, *ed = NULL, drft[BUFSIZ], buf[BUFSIZ];
    char **argp, **arguments;
    struct msgs *mp = NULL;
    struct stat st;
    FILE *in;

#ifdef MHE
    int buildsw = 0;
#endif /* MHE */

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
		    snprintf (buf, sizeof(buf), "%s: [+folder] [msg] [switches]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case GROUPSW:
		    groupreply++;
		    continue;
		case NGROUPSW:
		    groupreply = 0;
		    continue;

		case ANNOSW: 
		    anot++;
		    continue;
		case NANNOSW: 
		    anot = 0;
		    continue;

		case CCSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    docc (cp, 1);
		    continue;
		case NCCSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    docc (cp, 0);
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
#ifdef MHE
		case BILDSW: 
		    buildsw++;	/* fall... */
#endif /* MHE */
		case NWHATSW: 
		    nwhat++;
		    continue;

		case FCCSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    dp = NULL;
		    if (*cp == '@')
			cp = dp = path (cp + 1, TSUBCWF);
		    if (fcc)
			fcc = add (", ", fcc);
		    fcc = add (cp, fcc);
		    if (dp)
			free (dp);
		    continue;

		case FILESW: 
		    if (file)
			adios (NULL, "only one file at a time!");
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    file = path (cp, TFILE);
		    continue;
		case FILTSW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    filter = getcpy (etcpath (cp));
		    mime = 0;
		    continue;
		case FORMSW: 
		    if (!(form = *argp++) || *form == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case FRMTSW: 
		    filter = getcpy (etcpath (mhlreply));
		    mime = 0;
		    continue;
		case NFRMTSW: 
		    filter = NULL;
		    continue;

		case INPLSW: 
		    inplace++;
		    continue;
		case NINPLSW: 
		    inplace = 0;
		    continue;

		case MIMESW:
		    mime++;
		    filter = NULL;
		    continue;
		case NMIMESW:
		    mime = 0;
		    continue;

		case QURYSW: 
		    querysw++;
		    continue;
		case NQURYSW: 
		    querysw = 0;
		    continue;

		case WIDTHSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((outputlinelen = atoi (cp)) < 10)
			adios (NULL, "impossible width %d", outputlinelen);
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
		folder = path (cp + 1, *cp == '+' ? TFOLDER : TSUBCWF);
	} else {
	    if (msg)
		adios (NULL, "only one message at a time!");
	    else
		msg = cp;
	}
    }

    if (ccto == -1) 
	ccto = groupreply;
    if (cccc == -1)
	cccc = groupreply;
    if (ccme == -1)
	ccme = groupreply;

    cwd = getcpy (pwd ());

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (file && (msg || folder))
	adios (NULL, "can't mix files and folders/msgs");

try_it_again:

#ifdef MHE
    strncpy (drft, buildsw ? m_maildir ("reply")
			  : m_draft (dfolder, NULL, NOUSE, &isdf), sizeof(drft));

    /* Check if a draft exists */
    if (!buildsw && stat (drft, &st) != NOTOK) {
#else
    strncpy (drft, m_draft (dfolder, dmsg, NOUSE, &isdf), sizeof(drft));

    /* Check if a draft exists */
    if (stat (drft, &st) != NOTOK) {
#endif /* MHE */
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

    if (file) {
	/*
	 * We are replying to a file.
	 */
	anot = 0;	/* we don't want to annotate a file */
    } else {
	/*
	 * We are replying to a message.
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

	context_replace (pfolder, folder);	/* update current folder   */
	seq_setcur (mp, mp->lowsel);	/* update current message  */
	seq_save (mp);			/* synchronize sequences   */
	context_save ();			/* save the context file   */
    }

    msg = file ? file : getcpy (m_name (mp->lowsel));

    if ((in = fopen (msg, "r")) == NULL)
	adios (msg, "unable to open");

    /* find form (components) file */
    if (!form) {
	if (groupreply)
	    form = etcpath (replgroupcomps);
	else
	    form = etcpath (replcomps);
    }

    replout (in, msg, drft, mp, outputlinelen, mime, form, filter, fcc);
    fclose (in);

    if (nwhat)
	done (0);
    what_now (ed, nedit, NOUSE, drft, msg, 0, mp,
	    anot ? "Replied" : NULL, inplace, cwd);
    return done (1);
}

void
docc (char *cp, int ccflag)
{
    switch (smatch (cp, ccswitches)) {
	case AMBIGSW: 
	    ambigsw (cp, ccswitches);
	    done (1);
	case UNKWNSW: 
	    adios (NULL, "-%scc %s unknown", ccflag ? "" : "no", cp);

	case CTOSW: 
	    ccto = ccflag;
	    break;

	case CCCSW: 
	    cccc = ccflag;
	    break;

	case CMESW: 
	    ccme = ccflag;
	    break;

	case CALSW: 
	    ccto = cccc = ccme = ccflag;
	    break;
    }
}
