/* comp.c -- compose a message
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <h/fmt_scan.h>
#include "h/done.h"
#include "sbr/m_maildir.h"
#include <fcntl.h>

#define COMP_SWITCHES \
    X("draftfolder +folder", 0, DFOLDSW) \
    X("draftmessage msg", 0, DMSGSW) \
    X("nodraftfolder", 0, NDFLDSW) \
    X("editor editor", 0, EDITRSW) \
    X("noedit", 0, NEDITSW) \
    X("file file", 0, FILESW) \
    X("form formfile", 0, FORMSW) \
    X("use", 0, USESW) \
    X("nouse", 0, NUSESW) \
    X("whatnowproc program", 0, WHATSW) \
    X("nowhatnowproc", 0, NWHATSW) \
    X("build", 5, BILDSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("to address", 0, TOSW) \
    X("cc address", 0, CCSW) \
    X("from address", 0, FROMSW) \
    X("fcc mailbox", 0, FCCSW) \
    X("width columns", 0, WIDTHSW) \
    X("subject text", 0, SUBJECTSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(COMP);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(COMP, switches);
#undef X

#define DISPO_SWITCHES \
    X("quit", 0, NOSW) \
    X("replace", 0, YESW) \
    X("use", 0, USELSW) \
    X("list", 0, LISTDSW) \
    X("refile +folder", 0, REFILSW) \
    X("new", 0, NEWSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(DISPO);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(DISPO, aqrunl);
#undef X

static struct swit aqrul[] = {
    { "quit", 0, NOSW },
    { "replace", 0, YESW },
    { "use", 0, USELSW },
    { "list", 0, LISTDSW },
    { "refile", 0, REFILSW },
    { NULL, 0, 0 }
};

int
main (int argc, char **argv)
{
    int use = NOUSE, nedit = 0, nwhat = 0, build = 0;
    int i, in = NOTOK, isdf = 0, out, dat[5], format_len = 0;
    int outputlinelen = OUTPUTLINELEN;
    char *cp, *cwd, *maildir, *dfolder = NULL;
    char *ed = NULL, *file = NULL, *form = NULL;
    char *folder = NULL, *msg = NULL, buf[BUFSIZ];
    char *to = NULL, *from = NULL, *cc = NULL, *fcc = NULL, *dp;
    char *subject = NULL;
    char drft[BUFSIZ], **argp, **arguments;
    struct msgs *mp = NULL;
    struct format *fmt;
    struct stat st;

    if (nmh_init(argv[0], true, true)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    die("-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [+folder] [msg] [switches]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

		case EDITRSW: 
		    if (!(ed = *argp++) || *ed == '-')
			die("missing argument to %s", argp[-2]);
		    nedit = 0;
		    continue;
		case NEDITSW: 
		    nedit++;
		    continue;

		case WHATSW: 
		    if (!(whatnowproc = *argp++) || *whatnowproc == '-')
			die("missing argument to %s", argp[-2]);
		    nwhat = 0;
		    continue;

		case BILDSW:
		    build++;
		    /* FALLTHRU */
		case NWHATSW: 
		    nwhat++;
		    continue;

		case FORMSW: 
		    if (!(form = *argp++) || *form == '-')
			die("missing argument to %s", argp[-2]);
		    continue;

		case USESW: 
		    use++;
		    continue;
		case NUSESW: 
		    use = NOUSE;
		    continue;

		case FILESW: 	/* compatibility */
		    if (file)
			die("only one file at a time!");
		    if (!(file = *argp++) || *file == '-')
			die("missing argument to %s", argp[-2]);
		    isdf = NOTOK;
		    continue;

		case DFOLDSW: 
		    if (dfolder)
			die("only one draft folder at a time!");
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    dfolder = path (*cp == '+' || *cp == '@' ? cp + 1 : cp,
			    *cp != '@' ? TFOLDER : TSUBCWF);
		    continue;
		case DMSGSW: 
		    if (file)
			die("only one draft message at a time!");
		    if (!(file = *argp++) || *file == '-')
			die("missing argument to %s", argp[-2]);
		    continue;
		case NDFLDSW: 
		    dfolder = NULL;
		    isdf = NOTOK;
		    continue;

		case TOSW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    to = addlist(to, cp);
		    continue;

		case CCSW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    cc = addlist(cc, cp);
		    continue;

		case FROMSW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    from = addlist(from, cp);
		    continue;

		case FCCSW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    dp = NULL;
		    if (*cp == '@')
			cp = dp = path(cp + 1, TSUBCWF);
		    fcc = addlist(fcc, cp);
                    free(dp);
		    continue;

		case WIDTHSW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    if ((outputlinelen = atoi(cp)) < 10)
			die("impossible width %d", outputlinelen);
		    continue;

		case SUBJECTSW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    subject = cp;
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		die("only one folder at a time!");
            folder = pluspath (cp);
	} else {
	    if (msg)
		die("only one message at a time!");
            msg = cp;
	}
    }

    cwd = mh_xstrdup(pwd ());

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
	    die("can't mix forms and folders/msgs");

    cp = NULL;

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
	if (!(mp = folder_read (folder, 1)))
	    die("unable to read folder %s", folder);

	/* check for empty folder */
	if (mp->nummsg == 0)
	    die("no messages in %s", folder);

	/* parse the message range/sequence/name and set SELECTED */
	if (!m_convert (mp, msg))
	    done (1);
	seq_setprev (mp);	/* set the previous-sequence */
	seq_save (mp);

	if (mp->numsel > 1)
	    die("only one message at a time!");

	if ((in = open (form = mh_xstrdup(m_name (mp->lowsel)), O_RDONLY)) == NOTOK)
	    adios (form, "unable to open message");
    } else {
	struct comp *cptr;

    	if (! form)
	    form = components;

        cp = new_fs(form, NULL, NULL);
	format_len = strlen(cp);
	fmt_compile(cp, &fmt, 1);

	/*
	 * Set up any components that were fed to us on the command line
	 */

	if (from) {
	    cptr = fmt_findcomp("from");
	    if (cptr)
	    	cptr->c_text = from;
	}
	if (to) {
	    cptr = fmt_findcomp("to");
	    if (cptr)
	    	cptr->c_text = to;
	}
	if (cc) {
	    cptr = fmt_findcomp("cc");
	    if (cptr)
	    	cptr->c_text = cc;
	}
	if (fcc) {
	    cptr = fmt_findcomp("fcc");
	    if (cptr)
	    	cptr->c_text = fcc;
	}
	if (subject) {
	    cptr = fmt_findcomp("subject");
	    if (cptr)
	    	cptr->c_text = subject;
	}
    }

try_it_again:
    strncpy (drft, build ? m_maildir ("draft")
    			: m_draft (dfolder, file, use, &isdf), sizeof(drft));

    /*
     * Check if we have an existing draft
     */
    if (!build && (out = open (drft, O_RDONLY)) != NOTOK) {
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
	    if (!(argp = read_switch_multiword ("\nDisposition? ",
						isdf ? aqrunl : aqrul)))
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
		    inform("say what?");
		    break;
	    }
	}
    } else {
	if (use)
	    adios (drft, "unable to open");
    }

    if ((out = creat (drft, m_gmprot ())) == NOTOK)
	adios (drft, "unable to create");
    if (cp) {
	charstring_t scanl;

	i = format_len + 1024;
	scanl = charstring_create (i + 2);
	dat[0] = 0;
	dat[1] = 0;
	dat[2] = 0;
	dat[3] = outputlinelen;
	dat[4] = 0;
	fmt_scan(fmt, scanl, i, dat, NULL);
	if (write(out, charstring_buffer (scanl),
		  charstring_bytes (scanl)) < 0) {
	    advise (drft, "write");
	}
	charstring_free(scanl);
    } else {
	cpydata (in, out, form, drft);
	close (in);
    }
    close (out);

edit_it:
    context_save ();	/* save the context file */

    if (nwhat)
	done (0);
    what_now (ed, nedit, use, drft, NULL, 0, NULL, NULL, 0, cwd, 0);
    done (1);
    return 1;
}
