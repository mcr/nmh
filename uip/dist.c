/* dist.c -- re-distribute a message
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include "sbr/m_maildir.h"
#include <fcntl.h>
#include "h/done.h"

#define DIST_SWITCHES \
    X("annotate", 0, ANNOSW) \
    X("noannotate", 0, NANNOSW) \
    X("draftfolder +folder", 0, DFOLDSW) \
    X("draftmessage msg", 0, DMSGSW) \
    X("nodraftfolder", 0, NDFLDSW) \
    X("editor editor", 0, EDITRSW) \
    X("noedit", 0, NEDITSW) \
    X("form formfile", 0, FORMSW) \
    X("inplace", 0, INPLSW) \
    X("noinplace", 0, NINPLSW) \
    X("whatnowproc program", 0, WHATSW) \
    X("nowhatnowproc", 0, NWHATSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("file file", -4, FILESW) \
    X("from address", 0, FROMSW) \
    X("to address", 0, TOSW) \
    X("cc address", 0, CCSW) \
    X("fcc mailbox", 0, FCCSW) \
    X("width columns", 0, WIDTHSW) \
    X("atfile", 0, ATFILESW) \
    X("noatfile", 0, NOATFILESW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(DIST);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(DIST, switches);
#undef X

#define DISPO_SWITCHES \
    X("quit", 0, NOSW) \
    X("replace", 0, YESW) \
    X("list", 0, LISTDSW) \
    X("refile +folder", 0, REFILSW) \
    X("new", 0, NEWSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(DISPO);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(DISPO, aqrnl);
#undef X


static struct swit aqrl[] = {
    { "quit", 0, NOSW },
    { "replace", 0, YESW },
    { "list", 0, LISTDSW },
    { "refile +folder", 0, REFILSW },
    { NULL, 0, 0 }
};


int
main (int argc, char **argv)
{
    int anot = 0, inplace = 1, nedit = 0;
    int nwhat = 0, i, in, isdf = 0, out;
    int outputlinelen = OUTPUTLINELEN;
    int dat[5], atfile = 0;
    char *cp, *cwd, *maildir, *msgnam, *dfolder = NULL;
    char *dmsg = NULL, *ed = NULL, *file = NULL, *folder = NULL;
    char *form = NULL, *msg = NULL, buf[BUFSIZ], drft[BUFSIZ];
    char *from = NULL, *to = NULL, *cc = NULL, *fcc = NULL;
    char **argp, **arguments;
    struct msgs *mp = NULL;
    struct stat st;

    if (nmh_init(argv[0], 1)) { return 1; }

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
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

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

		case FROMSW:
		    if (!(cp = *argp++) || *cp == '-')
		    	adios (NULL, "missing argument to %s", argp[-2]);
		    from = addlist(from, cp);
		    continue;
		case TOSW:
		    if (!(cp = *argp++) || *cp == '-')
		    	adios (NULL, "missing argument to %s", argp[-2]);
		    to = addlist(to, cp);
		    continue;
		case CCSW:
		    if (!(cp = *argp++) || *cp == '-')
		    	adios (NULL, "missing argument to %s", argp[-2]);
		    cc = addlist(cc, cp);
		    continue;
		case FCCSW:
		    if (!(cp = *argp++) || *cp == '-')
		    	adios (NULL, "missing argument to %s", argp[-2]);
		    fcc = addlist(fcc, cp);
		    continue;

		case WIDTHSW:
		    if (!(cp = *argp++) || *cp == '-')
		    	adios (NULL, "missing argument to %s", argp[-2]);
		    if ((outputlinelen = atoi(cp)) < 10)
		    	adios (NULL, "impossible width %d", outputlinelen);
		    continue;

		case ATFILESW:
		    atfile++;
		    continue;
		case NOATFILESW:
		    atfile = 0;
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		adios (NULL, "only one folder at a time!");
            folder = pluspath (cp);
	} else {
	    if (msg)
		adios (NULL, "only one message at a time!");
            msg = cp;
	}
    }

    cwd = mh_xstrdup(pwd ());

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (file && (msg || folder))
	adios (NULL, "can't mix files and folders/msgs");

try_it_again:
    strncpy (drft, m_draft (dfolder, dmsg, NOUSE, &isdf), sizeof(drft));

    /* Check if draft already exists */
    if (stat (drft, &st) != NOTOK) {
	printf ("Draft \"%s\" exists (%ld bytes).", drft, (long) st.st_size);
	for (i = LISTDSW; i != YESW;) {
	    if (!(argp = read_switch_multiword ("\nDisposition? ",
						isdf ? aqrnl : aqrl)))
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
		    inform("say what?");
		    break;
	    }
	}
    }

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
	if (!(mp = folder_read (folder, 1)))
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

    msgnam = file ? file : mh_xstrdup(m_name (mp->lowsel));

    dat[0] = mp ? mp->lowsel : 0;
    dat[1] = 0;
    dat[2] = 0;
    dat[3] = outputlinelen;
    dat[4] = 0;

    if (!form)
    	form = distcomps;

    in = build_form(form, NULL, dat, from, to, cc, fcc, NULL, msgnam);

    if ((out = creat (drft, m_gmprot ())) == NOTOK)
	adios (drft, "unable to create");

    cpydata (in, out, form, drft);
    close (in);
    close (out);

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
    what_now (ed, nedit, NOUSE, drft, msgnam, 1, mp, anot ? "Resent" : NULL,
	    inplace, cwd, atfile);
    done (1);
    return 1;
}
