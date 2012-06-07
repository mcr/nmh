
/*
 * show.c -- show/list messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/mime.h>
#include <h/utils.h>

static struct swit switches[] = {
#define CHECKMIMESW          0
    { "checkmime", 0 },
#define NOCHECKMIMESW        1
    { "nocheckmime", 0 },
#define	HEADSW               2
    { "header", 0 },
#define	NHEADSW              3
    { "noheader", 0 },
#define	FORMSW               4
    { "form formfile", 0 },
#define	PROGSW               5
    { "moreproc program", 0 },
#define	NPROGSW              6
    { "nomoreproc", 0 },
#define	LENSW                7
    { "length lines", 0 },
#define	WIDTHSW              8
    { "width columns", 0 },
#define	SHOWSW               9
    { "showproc program", 0 },
#define SHOWMIMESW          10
    { "showmimeproc program", 0 },
#define	NSHOWSW             11
    { "noshowproc", 0 },
#define	DRFTSW              12
    { "draft", 0 },
#define	FILESW              13
    { "file file", -4 },		/* interface from showfile */
#define FMTPROCSW           14
    { "fmtproc program", 0 },
#define NFMTPROCSW          15
    { "nofmtproc", 0 },
#define VERSIONSW           16
    { "version", 0 },
#define	HELPSW              17
    { "help", 0 },
    { NULL, 0 }
};

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
    int draftsw = 0, headersw = 1, msgp = 0;
    int nshow = 0, checkmime = 1, mime;
    int vecp = 1, procp = 1, isdf = 0, mode = SHOW, msgnum;
    char *cp, *maildir, *file = NULL, *folder = NULL, *proc;
    char buf[BUFSIZ], **argp, **arguments;
    char *msgs[MAXARGS], *vec[MAXARGS];
    struct msgs *mp = NULL;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    if (!mh_strcasecmp (invo_name, "next")) {
	mode = NEXT;
    } else if (!mh_strcasecmp (invo_name, "prev")) {
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
		case UNKWNSW: 
		case NPROGSW:
		case NFMTPROCSW:
		    vec[vecp++] = --cp;
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
			adios (NULL, "only one file at a time!");
		    draftsw++;
		    if (mode == SHOW)
			continue;
usage:
		    adios (NULL,
			    "usage: %s [+folder] [switches] [switches for showproc]",
			    invo_name);
		case FILESW: 
		    if (mode != SHOW)
			goto usage;
		    if (draftsw || file)
			adios (NULL, "only one file at a time!");
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    file = path (cp, TFILE);
		    continue;

		case HEADSW: 
		    headersw++;
		    continue;
		case NHEADSW: 
		    headersw = 0;
		    continue;

		case FORMSW:
		    vec[vecp++] = --cp;
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    vec[vecp++] = getcpy (etcpath(cp));
		    continue;

		case PROGSW:
		case LENSW:
		case WIDTHSW:
		case FMTPROCSW:
		    vec[vecp++] = --cp;
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    vec[vecp++] = cp;
		    continue;

		case SHOWSW: 
		    if (!(showproc = *argp++) || *showproc == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    nshow = 0;
		    continue;
		case NSHOWSW: 
		    nshow++;
		    continue;

		case SHOWMIMESW:
		    if (!(showmimeproc = *argp++) || *showmimeproc == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    nshow = 0;
		    continue;
		case CHECKMIMESW:
		    checkmime++;
		    continue;
		case NOCHECKMIMESW:
		    checkmime = 0;
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		adios (NULL, "only one folder at a time!");
	    else
		folder = pluspath (cp);
	} else {
	    if (mode != SHOW)
		goto usage;
	    else
		msgs[msgp++] = cp;
	}
    }
    procp = vecp;

    if (!context_find ("path"))
	free (path ("./", TFOLDER));

    if (draftsw || file) {
	if (msgp)
	    adios (NULL, "only one file at a time!");
	vec[vecp++] = draftsw
	    ? getcpy (m_draft (folder, msgp ? msgs[0] : NULL, 1, &isdf))
	    : file;
	goto go_to_it;
    }

#ifdef WHATNOW
    if (!msgp && !folder && mode == SHOW && (cp = getenv ("mhdraft")) && *cp) {
	draftsw++;
	vec[vecp++] = cp;
	goto go_to_it;
    }
#endif /* WHATNOW */

    if (!msgp) {
	switch (mode) {
	    case NEXT:
		msgs[msgp++] = "next";
		break;
	    case PREV:
		msgs[msgp++] = "prev";
		break;
	    default:
		msgs[msgp++] = "cur";
		break;
	}
    }

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

    /* parse all the message ranges/sequences and set SELECTED */
    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
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

    if (mp->numsel > MAXARGS - 2)
	adios (NULL, "more than %d messages for show exec", MAXARGS - 2);

    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	if (is_selected(mp, msgnum))
	    vec[vecp++] = getcpy (m_name (msgnum));

    seq_setcur (mp, mp->hghsel);	/* update current message  */
    seq_save (mp);			/* synchronize sequences   */
    context_replace (pfolder, folder);	/* update current folder   */
    context_save ();			/* save the context file   */

    if (headersw && vecp == 2)
	printf ("(Message %s:%s)\n", folder, vec[1]);

go_to_it: ;
    fflush (stdout);

    vec[vecp] = NULL;

    /*
     * Decide which "proc" to use
     */
    mime = 0;
    if (nshow) {
	proc = catproc;
    } else {
	/* check if any messages are non-text MIME messages */
	if (checkmime && !getenv ("NOMHNPROC")) {
	    if (!draftsw && !file) {
		/* loop through selected messages and check for MIME */
		for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
		    if (is_selected (mp, msgnum) && is_nontext (m_name (msgnum))) {
			mime = 1;
			break;
		    }
	    } else {
		/* check the file or draft for MIME */
		if (is_nontext (vec[vecp - 1]))
		    mime = 1;
	    }
	}

	/* Set the "proc" */
	if (mime)
	    proc = showmimeproc;
	else
	    proc = showproc;
    }

    if (folder && !draftsw && !file)
	m_putenv ("mhfolder", folder);

    /*
     * For backward compatibility, if the "proc" is mhn,
     * then add "-show" option.  Add "-file" if showing
     * file or draft.
     */
    if (strcmp (r1bindex (proc, '/'), "mhn") == 0) {
	if (draftsw || file) {
	    vec[vecp] = vec[vecp - 1];
	    vec[vecp - 1] = "-file";
	    vecp++;
	}
	vec[vecp++] = "-show";
	vec[vecp] = NULL;
    }

    /* If the "proc" is "mhshow", add "-file" if showing file or draft.
     */
    if (strcmp (r1bindex (proc, '/'), "mhshow") == 0 && (draftsw || file) ) {
       vec[vecp] = vec[vecp - 1];
       vec[vecp - 1] = "-file";
       vec[++vecp] = NULL;
    }

    /*
     * If "proc" is mhl, then run it internally
     * rather than exec'ing it.
     */
    if (strcmp (r1bindex (proc, '/'), "mhl") == 0) {
	vec[0] = "mhl";
	mhl (vecp, vec);
	done (0);
    }

    /*
     * If you are not using a nmh command as your "proc", then
     * add the path to the message names.  Currently, we are just
     * checking for mhn here, since we've already taken care of mhl.
     */
    if (!strcmp (r1bindex (proc, '/'), "mhl")
	    && !draftsw
	    && !file
	    && chdir (maildir = concat (m_maildir (""), "/", NULL)) != NOTOK) {
	mp->foldpath = concat (mp->foldpath, "/", NULL);
	cp = ssequal (maildir, mp->foldpath)
	    ? mp->foldpath + strlen (maildir)
	    : mp->foldpath;
	for (msgnum = procp; msgnum < vecp; msgnum++)
	    vec[msgnum] = concat (cp, vec[msgnum], NULL);
    }

    vec[0] = r1bindex (proc, '/');
    execvp (proc, vec);
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
    unsigned char *bp, *dp;
    char *cp;
    char buf[BUFSIZ], name[NAMESZ];
    FILE *fp;

    if ((fp = fopen (msgnam, "r")) == NULL)
	return 0;

    for (state = FLD;;) {
	switch (state = m_getfld (state, name, buf, sizeof(buf), fp)) {
	case FLD:
	case FLDPLUS:
	case FLDEOF:
	    /*
	     * Check Content-Type field
	     */
	    if (!mh_strcasecmp (name, TYPE_FIELD)) {
		int passno;
		char c;

		cp = add (buf, NULL);
		while (state == FLDPLUS) {
		    state = m_getfld (state, name, buf, sizeof(buf), fp);
		    cp = add (buf, cp);
		}
		bp = cp;
		passno = 1;

again:
		for (; isspace (*bp); bp++)
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
			    /* and fall... */
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
		    if ((result = (mh_strcasecmp (bp, "plain") != 0)))
			goto out;
		    *dp = c;
		    for (dp++; isspace (*dp); dp++)
			continue;
		    if (*dp) {
			if ((result = !uprf (dp, "charset")))
			    goto out;
			dp += sizeof("charset") - 1;
			while (isspace (*dp))
			    dp++;
			if (*dp++ != '=')
			    goto invalid;
			while (isspace (*dp))
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
		    if (!(result = (mh_strcasecmp (bp, "text") != 0))) {
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
		    return result;
		}
		break;
	    }

	    /*
	     * Check Content-Transfer-Encoding field
	     */
	    if (!mh_strcasecmp (name, ENCODING_FIELD)) {
		cp = add (buf, NULL);
		while (state == FLDPLUS) {
		    state = m_getfld (state, name, buf, sizeof(buf), fp);
		    cp = add (buf, cp);
		}
		for (bp = cp; isspace (*bp); bp++)
		    continue;
		for (dp = bp; istoken (*dp); dp++)
		    continue;
		*dp = '\0';
		result = (mh_strcasecmp (bp, "7bit")
		       && mh_strcasecmp (bp, "8bit")
		       && mh_strcasecmp (bp, "binary"));

		free (cp);
		if (result) {
		    fclose (fp);
		    return result;
		}
		break;
	    }

	    /*
	     * Just skip the rest of this header
	     * field and go to next one.
	     */
	    while (state == FLDPLUS)
		state = m_getfld (state, name, buf, sizeof(buf), fp);
	    break;

	    /*
	     * We've passed the message header,
	     * so message is just text.
	     */
	default:
	    fclose (fp);
	    return 0;
	}
    }
}
