
/*
 * forw.c -- forward a message, or group of messages.
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/fmt_scan.h>
#include <h/tws.h>
#include <h/utils.h>


#define	IFORMAT	"digest-issue-%s"
#define	VFORMAT	"digest-volume-%s"

static struct swit switches[] = {
#define	ANNOSW                 0
    { "annotate", 0 },
#define	NANNOSW                1
    { "noannotate", 0 },
#define	DFOLDSW                2
    { "draftfolder +folder", 0 },
#define	DMSGSW                 3
    { "draftmessage msg", 0 },
#define	NDFLDSW                4
    { "nodraftfolder", 0 },
#define	EDITRSW                5
    { "editor editor", 0 },
#define	NEDITSW                6
    { "noedit", 0 },
#define	FILTSW                 7
    { "filter filterfile", 0 },
#define	FORMSW                 8
    { "form formfile", 0 },
#define	FRMTSW                 9
    { "format", 5 },
#define	NFRMTSW               10
    { "noformat", 7 },
#define	INPLSW                11
    { "inplace", 0 },
#define	NINPLSW               12
    { "noinplace", 0 },
#define MIMESW                13
    { "mime", 0 },
#define NMIMESW               14
    { "nomime", 0 },
#define	DGSTSW                15
    { "digest list", 0 },
#define	ISSUESW               16
    { "issue number", 0 },
#define	VOLUMSW               17
    { "volume number", 0 },
#define	WHATSW                18
    { "whatnowproc program", 0 },
#define	NWHATSW               19
    { "nowhatnowproc", 0 },
#define	BITSTUFFSW            20
    { "dashstuffing", 0 },		/* interface to mhl */
#define	NBITSTUFFSW           21
    { "nodashstuffing", 0 },
#define VERSIONSW             22
    { "version", 0 },
#define	HELPSW                23
    { "help", 0 },
#define	FILESW                24
    { "file file", 4 },			/* interface from msh */

#ifdef MHE
#define	BILDSW                25
    { "build", 5 },			/* interface from mhe */
#endif /* MHE */

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

static char drft[BUFSIZ];

static char delim3[] =
    "\n------------------------------------------------------------\n\n";
static char delim4[] = "\n------------------------------\n\n";


static struct msgs *mp = NULL;		/* used a lot */


/*
 * static prototypes
 */
static void mhl_draft (int, char *, int, int, char *, char *, int);
static void copy_draft (int, char *, char *, int, int, int);
static void copy_mime_draft (int);
static int build_form (char *, char *, int, int);


int
main (int argc, char **argv)
{
    int msgp = 0, anot = 0, inplace = 1, mime = 0;
    int issue = 0, volume = 0, dashstuff = 0;
    int nedit = 0, nwhat = 0, i, in;
    int out, isdf = 0, msgnum;
    char *cp, *cwd, *maildir, *dfolder = NULL;
    char *dmsg = NULL, *digest = NULL, *ed = NULL;
    char *file = NULL, *filter = NULL, *folder = NULL;
    char *form = NULL, buf[BUFSIZ], value[10];
    char **argp, **arguments, *msgs[MAXARGS];
    struct stat st;

#ifdef	MHE
    int buildsw = 0;
#endif	/* MHE */

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
		    snprintf (buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
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
#ifdef MHE
		case BILDSW:
		    buildsw++;	/* fall... */
#endif /* MHE */
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
		    filter = getcpy (etcpath (mhlforward));
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

		case DGSTSW: 
		    if (!(digest = *argp++) || *digest == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    mime = 0;
		    continue;
		case ISSUESW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((issue = atoi (cp)) < 1)
			adios (NULL, "bad argument %s %s", argp[-2], cp);
		    continue;
		case VOLUMSW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((volume = atoi (cp)) < 1)
			adios (NULL, "bad argument %s %s", argp[-2], cp);
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

		case BITSTUFFSW: 
		    dashstuff = 1;	/* trinary logic */
		    continue;
		case NBITSTUFFSW: 
		    dashstuff = -1;	/* trinary logic */
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		adios (NULL, "only one folder at a time!");
	    else
		folder = pluspath (cp);
	} else {
	    msgs[msgp++] = cp;
	}
    }

    cwd = getcpy (pwd ());

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (file && (msgp || folder))
	adios (NULL, "can't mix files and folders/msgs");

try_it_again:

#ifdef MHE
    strncpy (drft, buildsw ? m_maildir ("draft")
			  : m_draft (dfolder, NULL, NOUSE, &isdf), sizeof(drft));

    /* Check if a draft already exists */
    if (!buildsw && stat (drft, &st) != NOTOK) {
#else
    strncpy (drft, m_draft (dfolder, dmsg, NOUSE, &isdf), sizeof(drft));

    /* Check if a draft already exists */
    if (stat (drft, &st) != NOTOK) {
#endif	/* MHE */
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
	 * Forwarding a file.
         */
	anot = 0;	/* don't want to annotate a file */
    } else {
	/*
	 * Forwarding a message.
	 */
	if (!msgp)
	    msgs[msgp++] = "cur";
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
	seq_setprev (mp);	/* set the previous sequence */
    }

    if (filter && access (filter, R_OK) == NOTOK)
	adios (filter, "unable to read");

    /*
     * Open form (component) file.
     */
    if (digest) {
	if (issue == 0) {
	    snprintf (buf, sizeof(buf), IFORMAT, digest);
	    if (volume == 0
		    && (cp = context_find (buf))
		    && ((issue = atoi (cp)) < 0))
		issue = 0;
	    issue++;
	}
	if (volume == 0)
	    snprintf (buf, sizeof(buf), VFORMAT, digest);
	if ((cp = context_find (buf)) == NULL || (volume = atoi (cp)) <= 0)
	    volume = 1;
	if (!form)
	    form = digestcomps;
	in = build_form (form, digest, volume, issue);
    } else
	in = open_form(&form, forwcomps);

    if ((out = creat (drft, m_gmprot ())) == NOTOK)
	adios (drft, "unable to create");

    /*
     * copy the components into the draft
     */
    cpydata (in, out, form, drft);
    close (in);

    if (file) {
	/* just copy the file into the draft */
	if ((in = open (file, O_RDONLY)) == NOTOK)
	    adios (file, "unable to open");
	cpydata (in, out, file, drft);
	close (in);
	close (out);
    } else {
	/*
	 * If filter file is defined, then format the
	 * messages into the draft using mhlproc.
	 */
	if (filter)
	    mhl_draft (out, digest, volume, issue, drft, filter, dashstuff);
	else if (mime)
	    copy_mime_draft (out);
	else
	    copy_draft (out, digest, drft, volume, issue, dashstuff);
	close (out);

	if (digest) {
	    snprintf (buf, sizeof(buf), IFORMAT, digest);
	    snprintf (value, sizeof(value), "%d", issue);
	    context_replace (buf, getcpy (value));
	    snprintf (buf, sizeof(buf), VFORMAT, digest);
	    snprintf (value, sizeof(value), "%d", volume);
	    context_replace (buf, getcpy (value));
	}

	context_replace (pfolder, folder);	/* update current folder   */
	seq_setcur (mp, mp->lowsel);		/* update current message  */
	seq_save (mp);				/* synchronize sequences   */
	context_save ();			/* save the context file   */
    }

    if (nwhat)
	done (0);
    what_now (ed, nedit, NOUSE, drft, NULL, 0, mp,
	anot ? "Forwarded" : NULL, inplace, cwd);
    done (1);
    return 1;
}


/*
 * Filter the messages you are forwarding, into the
 * draft calling the mhlproc, and reading its output
 * from a pipe.
 */

static void
mhl_draft (int out, char *digest, int volume, int issue,
            char *file, char *filter, int dashstuff)
{
    pid_t child_id;
    int i, msgnum, pd[2];
    char *vec[MAXARGS];
    char buf1[BUFSIZ];
    char buf2[BUFSIZ];
    
    if (pipe (pd) == NOTOK)
	adios ("pipe", "unable to create");

    vec[0] = r1bindex (mhlproc, '/');

    for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
	sleep (5);
    switch (child_id) {
	case NOTOK: 
	    adios ("fork", "unable to");

	case OK: 
	    close (pd[0]);
	    dup2 (pd[1], 1);
	    close (pd[1]);

	    i = 1;
	    vec[i++] = "-forwall";
	    vec[i++] = "-form";
	    vec[i++] = filter;

	    if (digest) {
		vec[i++] = "-digest";
		vec[i++] = digest;
		vec[i++] = "-issue";
		snprintf (buf1, sizeof(buf1), "%d", issue);
		vec[i++] = buf1;
		vec[i++] = "-volume";
		snprintf (buf2, sizeof(buf2), "%d", volume);
		vec[i++] = buf2;
	    }

	    /*
	     * Are we dashstuffing (quoting) the lines that begin
	     * with `-'.  We use the mhl default (don't add any flag)
	     * unless the user has specified a specific flag.
	     */
	    if (dashstuff > 0)
		vec[i++] = "-dashstuffing";
	    else if (dashstuff < 0)
		vec[i++] = "-nodashstuffing";

	    if (mp->numsel >= MAXARGS - i)
		adios (NULL, "more than %d messages for %s exec",
			MAXARGS - i, vec[0]);

	    /*
	     * Now add the message names to filter.  We can only
	     * handle about 995 messages (because vec is fixed size),
	     * but that should be plenty.
	     */
	    for (msgnum = mp->lowsel; msgnum <= mp->hghsel && i < sizeof(vec) - 1;
			msgnum++)
		if (is_selected (mp, msgnum))
		    vec[i++] = getcpy (m_name (msgnum));
	    vec[i] = NULL;

	    execvp (mhlproc, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (mhlproc);
	    _exit (-1);

	default: 
	    close (pd[1]);
	    cpydata (pd[0], out, vec[0], file);
	    close (pd[0]);
	    pidXwait(child_id, mhlproc);
	    break;
    }
}


/*
 * Copy the messages into the draft.  The messages are
 * not filtered through the mhlproc.  Do dashstuffing if
 * necessary.
 */

static void
copy_draft (int out, char *digest, char *file, int volume, int issue, int dashstuff)
{
    int fd,i, msgcnt, msgnum;
    int len, buflen;
    register char *bp, *msgnam;
    char buffer[BUFSIZ];

    msgcnt = 1;
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (is_selected (mp, msgnum)) {
	    if (digest) {
		strncpy (buffer, msgnum == mp->lowsel ? delim3 : delim4,
			sizeof(buffer));
	    } else {
		/* Get buffer ready to go */
		bp = buffer;
		buflen = sizeof(buffer);

		strncpy (bp, "\n-------", buflen);
		len = strlen (bp);
		bp += len;
		buflen -= len;

		if (msgnum == mp->lowsel) {
		    snprintf (bp, buflen, " Forwarded Message%s",
			mp->numsel > 1 ? "s" : "");
		} else {
		    snprintf (bp, buflen, " Message %d", msgcnt);
		}
		len = strlen (bp);
		bp += len;
		buflen -= len;

		strncpy (bp, "\n\n", buflen);
	    }
	    write (out, buffer, strlen (buffer));

	    if ((fd = open (msgnam = m_name (msgnum), O_RDONLY)) == NOTOK) {
		admonish (msgnam, "unable to read message");
		continue;
	    }

	    /*
	     * Copy the message.  Add RFC934 quoting (dashstuffing)
	     * unless given the -nodashstuffing flag.
	     */
	    if (dashstuff >= 0)
		cpydgst (fd, out, msgnam, file);
	    else
		cpydata (fd, out, msgnam, file);

	    close (fd);
	    msgcnt++;
	}
    }

    if (digest) {
	strncpy (buffer, delim4, sizeof(buffer));
    } else {
	snprintf (buffer, sizeof(buffer), "\n------- End of Forwarded Message%s\n\n",
		mp->numsel > 1 ? "s" : "");
    }
    write (out, buffer, strlen (buffer));

    if (digest) {
	snprintf (buffer, sizeof(buffer), "End of %s Digest [Volume %d Issue %d]\n",
		digest, volume, issue);
	i = strlen (buffer);
	for (bp = buffer + i; i > 1; i--)
	    *bp++ = '*';
	*bp++ = '\n';
	*bp = 0;
	write (out, buffer, strlen (buffer));
    }
}


/*
 * Create a mhn composition file for forwarding message.
 */

static void
copy_mime_draft (int out)
{
    int msgnum;
    char buffer[BUFSIZ];

    snprintf (buffer, sizeof(buffer), "#forw [forwarded message%s] +%s",
	mp->numsel == 1 ? "" : "s", mp->foldpath);
    write (out, buffer, strlen (buffer));
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	if (is_selected (mp, msgnum)) {
	    snprintf (buffer, sizeof(buffer), " %s", m_name (msgnum));
	    write (out, buffer, strlen (buffer));
	}
    write (out, "\n", 1);
}


static int
build_form (char *form, char *digest, int volume, int issue)
{
    int	in;
    int fmtsize;
    register char *nfs;
    char *line, tmpfil[BUFSIZ];
    FILE *tmp;
    register struct comp *cptr;
    struct format *fmt;
    int dat[5];
    char *cp = NULL;

    /* Get new format string */
    nfs = new_fs (form, NULL, NULL);
    fmtsize = strlen (nfs) + 256;

    /* Compile format string */
    fmt_compile (nfs, &fmt);

    FINDCOMP (cptr, "digest");
    if (cptr)
	cptr->c_text = digest;
    FINDCOMP (cptr, "date");
    if (cptr)
	cptr->c_text = getcpy(dtimenow (0));

    dat[0] = issue;
    dat[1] = volume;
    dat[2] = 0;
    dat[3] = fmtsize;
    dat[4] = 0;

    cp = m_mktemp2(NULL, invo_name, NULL, &tmp);
    if (cp == NULL) adios("forw", "unable to create temporary file");
    strncpy (tmpfil, cp, sizeof(tmpfil));
    unlink (tmpfil);
    if ((in = dup (fileno (tmp))) == NOTOK)
	adios ("dup", "unable to");

    line = mh_xmalloc ((unsigned) fmtsize);
    fmt_scan (fmt, line, fmtsize, dat);
    fputs (line, tmp);
    free (line);
    if (fclose (tmp))
	adios (tmpfil, "error writing");

    lseek (in, (off_t) 0, SEEK_SET);
    return in;
}
