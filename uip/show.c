/* show.c -- show/list messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/m_convert.h"
#include "sbr/getfolder.h"
#include "sbr/folder_read.h"
#include "sbr/context_save.h"
#include "sbr/context_replace.h"
#include "sbr/context_find.h"
#include "sbr/ambigsw.h"
#include "sbr/path.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/arglist.h"
#include "sbr/error.h"
#include "h/mime.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/m_maildir.h"

#define SHOW_SWITCHES \
    X("checkmime", 0, CHECKMIMESW) \
    X("nocheckmime", 0, NOCHECKMIMESW) \
    X("header", 0, HEADSW) \
    X("noheader", 0, NHEADSW) \
    X("form formfile", 0, FORMSW) \
    X("moreproc program", 0, PROGSW) \
    X("nomoreproc", 0, NPROGSW) \
    X("length lines", 0, LENSW) \
    X("width columns", 0, WIDTHSW) \
    X("showproc program", 0, SHOWSW) \
    X("showmimeproc program", 0, SHOWMIMESW) \
    X("noshowproc", 0, NSHOWSW) \
    X("draft", 0, DRFTSW) \
    X("file file", -4, FILESW) /* interface from showfile */ \
    X("fmtproc program", 0, FMTPROCSW) \
    X("nofmtproc", 0, NFMTPROCSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    /*				\
     * switches for mhlproc	\
     */				\
    X("concat", 0, CONCATSW) \
    X("noconcat", 0, NCONCATSW) \
    /*				\
     * switches for mhshow	\
     */				\
    X("part number", 0, PARTSW) \
    X("type content", 0, TYPESW) \
    X("prefer content", 0, PREFERSW) \
    X("markform file", 0, MARKFORMSW) \
    X("rcache policy", 0, RCACHESW) \
    X("wcache policy", 0, WCACHESW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(SHOW);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(SHOW, switches);
#undef X

/*
 * static prototypes
 */
static int is_nontext(char *);

#define	SHOW  0
#define	NEXT  1
#define	PREV  2


int
main (int argc, char **argv)
{
    bool draftsw = false;
    bool headersw = true;
    bool nshow = false;
    bool checkmime = true;
    bool mime = false;
    int isdf = 0, mode = SHOW, msgnum;
    char *cp, *maildir, *file = NULL, *folder = NULL, *proc, *program;
    char buf[BUFSIZ], **argp, **arguments;
    struct msgs *mp = NULL;
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs_array vec = { 0, 0, NULL }, non_mhl_vec = { 0, 0, NULL };

    if (nmh_init(argv[0], true, true)) { return 1; }

    if (!strcasecmp (invo_name, "next")) {
	mode = NEXT;
    } else if (!strcasecmp (invo_name, "prev")) {
	mode = PREV;
    }
    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW:
		    ambigsw (cp, switches);
		    done (1);

		case HEADSW:
		    headersw = true;
		    goto non_mhl_switches;
		case NHEADSW:
		    headersw = false;
		    /* FALLTHRU */
		case CONCATSW:
		case NCONCATSW:
non_mhl_switches:
		    /* mhl can't handle these, so keep them separate. */
		    app_msgarg(&non_mhl_vec, --cp);
		    continue;

		case UNKWNSW:
		case NPROGSW:
		case NFMTPROCSW:
		    app_msgarg(&vec, --cp);
		    continue;

		case HELPSW:
		    snprintf (buf, sizeof(buf),
			"%s [+folder] %s[switches] [switches for showproc]",
			invo_name, mode == SHOW ? "[msgs] ": "");
		    print_help (buf, switches, 1);
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

		case DRFTSW:
		    if (file)
			die("only one file at a time!");
		    draftsw = true;
		    if (mode == SHOW)
			continue;
usage:
		    die(			    "usage: %s [+folder] [switches] [switches for showproc]",
			    invo_name);
		case FILESW:
		    if (mode != SHOW)
			goto usage;
		    if (draftsw || file)
			die("only one file at a time!");
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    file = path (cp, TFILE);
		    continue;

		case FORMSW:
		    app_msgarg(&vec, --cp);
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    app_msgarg(&vec, mh_xstrdup(etcpath(cp)));
		    continue;

		case PROGSW:
		case LENSW:
		case WIDTHSW:
		case FMTPROCSW:
		case PARTSW:
		case TYPESW:
		case PREFERSW:
		case MARKFORMSW:
		case RCACHESW:
		case WCACHESW:
		    app_msgarg(&vec, --cp);
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    app_msgarg(&vec, cp);
		    continue;

		case SHOWSW:
		    if (!(showproc = *argp++) || *showproc == '-')
			die("missing argument to %s", argp[-2]);
		    nshow = false;
		    continue;
		case NSHOWSW:
		    nshow = true;
		    continue;

		case SHOWMIMESW:
		    if (!(showmimeproc = *argp++) || *showmimeproc == '-')
			die("missing argument to %s", argp[-2]);
		    nshow = false;
		    continue;
		case CHECKMIMESW:
		    checkmime = true;
		    continue;
		case NOCHECKMIMESW:
		    checkmime = false;
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		die("only one folder at a time!");
            folder = pluspath (cp);
	} else {
	    if (mode != SHOW)
		goto usage;
            app_msgarg(&msgs, cp);
	}
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));

    if (draftsw || file) {
	if (msgs.size)
	    die("only one file at a time!");
	if (draftsw)
	    app_msgarg(&vec, mh_xstrdup(m_draft(folder, NULL, 1, &isdf)));
	else
	    app_msgarg(&vec, file);
	headersw = false;
	goto go_to_it;
    }

    if (!msgs.size) {
	switch (mode) {
	    case NEXT:
		app_msgarg(&msgs, "next");
		break;
	    case PREV:
		app_msgarg(&msgs, "prev");
		break;
	    default:
		app_msgarg(&msgs, "cur");
		break;
	}
    }

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

    /* parse all the message ranges/sequences and set SELECTED */
    for (msgnum = 0; msgnum < msgs.size; msgnum++)
	if (!m_convert (mp, msgs.msgs[msgnum]))
	    done (1);

    /*
     * Set the SELECT_UNSEEN bit for all the SELECTED messages,
     * since we will use that as a tag to know which messages
     * to remove from the "unseen" sequence.
     */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	if (is_selected(mp, msgnum))
	    set_unseen (mp, msgnum);

    seq_setprev (mp);		/* set the Previous-Sequence */
    seq_setunseen (mp, 1);	/* unset the Unseen-Sequence */

    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	if (is_selected(mp, msgnum))
	    app_msgarg(&vec, mh_xstrdup(m_name (msgnum)));

    seq_setcur (mp, mp->hghsel);	/* update current message  */
    seq_save (mp);			/* synchronize sequences   */
    context_replace (pfolder, folder);	/* update current folder   */
    context_save ();			/* save the context file   */

go_to_it: ;

    /*
     * Decide which "proc" to use
     */
    if (nshow) {
	proc = catproc;
    } else {
	/* check if any messages are non-text MIME messages */
	if (! mime  &&  checkmime) {
	    if (!draftsw && !file) {
		/* loop through selected messages and check for MIME */
		for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
		    if (is_selected (mp, msgnum) && is_nontext (m_name (msgnum))) {
			mime = true;
			break;
		    }
	    } else {
		/* check the file or draft for MIME */
		if (is_nontext (vec.msgs[vec.size - 1]))
		    mime = true;
	    }
	}

	/* Set the "proc" */
	if (mime)
	    proc = showmimeproc;
	else
	    proc = showproc;
    }

    if (folder && !draftsw && !file)
	setenv("mhfolder", folder, 1);

    if (strcmp (r1bindex (proc, '/'), "cat") == 0) {

	if (headersw && vec.size == 1)
	    printf ("(Message %s:%s)\n", folder, vec.msgs[0]);

    } else if (strcmp (r1bindex (proc, '/'), "mhl") == 0) {

	if (headersw && vec.size == 1)
	    printf ("(Message %s:%s)\n", folder, vec.msgs[0]);

	/* If "mhl", then run it internally */
	argsplit_insert(&vec, "mhl", &program);
	app_msgarg(&vec, NULL);
	mhl (vec.size, vec.msgs);
	done (0);

    } else {
	int i;
	char **mp;

	for (i = 0, mp = non_mhl_vec.msgs; i < non_mhl_vec.size; ++i, ++mp) {
	    if (draftsw || file) {
		/* Insert the switch before the filename. */
		app_msgarg(&vec, vec.msgs[vec.size - 1]);
		vec.msgs[vec.size - 2] = *mp;
	    } else {
		app_msgarg(&vec, *mp);
	    }
	}

	if (strcmp (r1bindex (proc, '/'), "mhn") == 0) {
	    /* Add "-file" if showing file or draft, */
	    if (draftsw || file) {
		app_msgarg(&vec, vec.msgs[vec.size - 1]);
		vec.msgs[vec.size - 2] = "-file";
	    }
	    /* and add -show for backward compatibility */
	    app_msgarg(&vec, "-show");
	} else if (strcmp (r1bindex (proc, '/'), "mhshow") == 0) {
	    /* If "mhshow", add "-file" if showing file or draft. */
	    if (draftsw || file) {
		app_msgarg(&vec, vec.msgs[vec.size - 1]);
		vec.msgs[vec.size - 2] = "-file";
	    }
	} else {
	    if (headersw && vec.size == 1)
		printf ("(Message %s:%s)\n", folder, vec.msgs[0]);
	}
    }

    fflush (stdout);

    argsplit_insert(&vec, proc, &program);
    app_msgarg(&vec, NULL);
    execvp (program, vec.msgs);
    adios (proc, "unable to exec");
    return 0;  /* dead code to satisfy the compiler */
}


/*
 * Check if a message or file contains any non-text parts
 */
static int
is_nontext (char *msgnam)
{
    int	result, state;
    char *bp, *dp, *cp;
    char buf[NMH_BUFSIZ], name[NAMESZ];
    FILE *fp;
    m_getfld_state_t gstate;

    if ((fp = fopen (msgnam, "r")) == NULL)
	return 0;
    gstate = m_getfld_state_init(fp);
    for (;;) {
	int bufsz = sizeof buf;
	switch (state = m_getfld2(&gstate, name, buf, &bufsz)) {
	case FLD:
	case FLDPLUS:
	    /*
	     * Check Content-Type field
	     */
	    if (!strcasecmp (name, TYPE_FIELD)) {
		int passno;
		char c;

		cp = mh_xstrdup(buf);
		while (state == FLDPLUS) {
		    bufsz = sizeof buf;
		    state = m_getfld2(&gstate, name, buf, &bufsz);
		    cp = add (buf, cp);
		}
		bp = cp;
		passno = 1;

again:
		for (; isspace ((unsigned char) *bp); bp++)
		    continue;
		if (*bp == '(') {
		    int i;

		    for (bp++, i = 0;;) {
			switch (*bp++) {
			case '\0':
invalid:
			    result = 0;
			    goto out;
			case '\\':
			    if (*bp++ == '\0')
				goto invalid;
			    continue;
			case '(':
			    i++;
			    continue;
			default:
			    continue;
			case ')':
			    if (--i < 0)
				break;
			    continue;
			}
			break;
		    }
		}
		if (passno == 2) {
		    if (*bp != '/')
			goto invalid;
		    bp++;
		    passno = 3;
		    goto again;
		}
		for (dp = bp; istoken (*dp); dp++)
		    continue;
		c = *dp;
		*dp = '\0';
		if (!*bp)
		    goto invalid;
		if (passno > 1) {
		    if ((result = (strcasecmp (bp, "plain") != 0)))
			goto out;
		    *dp = c;
		    for (dp++; isspace ((unsigned char) *dp); dp++)
			continue;
		    if (*dp) {
			if ((result = !uprf (dp, "charset")))
			    goto out;
			dp += LEN("charset");
			while (isspace ((unsigned char) *dp))
			    dp++;
			if (*dp++ != '=')
			    goto invalid;
			while (isspace ((unsigned char) *dp))
			    dp++;
			if (*dp == '"') {
			    if ((bp = strchr(++dp, '"')))
				*bp = '\0';
			} else {
			    for (bp = dp; *bp; bp++)
				if (!istoken (*bp)) {
				    *bp = '\0';
				    break;
				}
			}
		    } else {
			/* Default character set */
			dp = "US-ASCII";
		    }
		    /* Check the character set */
		    result = !check_charset (dp, strlen (dp));
		} else {
		    if (!(result = (strcasecmp (bp, "text") != 0))) {
			*dp = c;
			bp = dp;
			passno = 2;
			goto again;
		    }
		}
out:
		free (cp);
		if (result) {
		    fclose (fp);
		    m_getfld_state_destroy (&gstate);
		    return result;
		}
		break;
	    }

	    /*
	     * Check Content-Transfer-Encoding field
	     */
	    if (!strcasecmp (name, ENCODING_FIELD)) {
		cp = mh_xstrdup(buf);
		while (state == FLDPLUS) {
		    bufsz = sizeof buf;
		    state = m_getfld2(&gstate, name, buf, &bufsz);
		    cp = add (buf, cp);
		}
		for (bp = cp; isspace ((unsigned char) *bp); bp++)
		    continue;
		for (dp = bp; istoken ((unsigned char) *dp); dp++)
		    continue;
		*dp = '\0';
		result = (strcasecmp (bp, "7bit")
		       && strcasecmp (bp, "8bit")
		       && strcasecmp (bp, "binary"));

		free (cp);
		if (result) {
		    fclose (fp);
		    m_getfld_state_destroy (&gstate);
		    return result;
		}
		break;
	    }

	    /*
	     * Just skip the rest of this header
	     * field and go to next one.
	     */
	    while (state == FLDPLUS) {
		bufsz = sizeof buf;
		state = m_getfld2(&gstate, name, buf, &bufsz);
	    }
	    break;

	    /*
	     * We've passed the message header,
	     * so message is just text.
	     */
	default:
	    fclose (fp);
	    m_getfld_state_destroy (&gstate);
	    return 0;
	}
    }
}
