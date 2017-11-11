/* forw.c -- forward a message, or group of messages.
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/m_draft.h"
#include "sbr/m_convert.h"
#include "sbr/getfolder.h"
#include "sbr/folder_read.h"
#include "sbr/context_save.h"
#include "sbr/context_replace.h"
#include "sbr/context_find.h"
#include "sbr/ambigsw.h"
#include "sbr/pidstatus.h"
#include "sbr/path.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/arglist.h"
#include "sbr/error.h"
#include <fcntl.h>
#include "h/tws.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/m_maildir.h"
#include "forwsbr.h"


#define	IFORMAT	"digest-issue-%s"
#define	VFORMAT	"digest-volume-%s"

#define FORW_SWITCHES \
    X("annotate", 0, ANNOSW) \
    X("noannotate", 0, NANNOSW) \
    X("draftfolder +folder", 0, DFOLDSW) \
    X("draftmessage msg", 0, DMSGSW) \
    X("nodraftfolder", 0, NDFLDSW) \
    X("editor editor", 0, EDITRSW) \
    X("noedit", 0, NEDITSW) \
    X("filter filterfile", 0, FILTSW) \
    X("form formfile", 0, FORMSW) \
    X("format", 5, FRMTSW) \
    X("noformat", 7, NFRMTSW) \
    X("inplace", 0, INPLSW) \
    X("noinplace", 0, NINPLSW) \
    X("mime", 0, MIMESW) \
    X("nomime", 0, NMIMESW) \
    X("digest list", 0, DGSTSW) \
    X("issue number", 0, ISSUESW) \
    X("volume number", 0, VOLUMSW) \
    X("whatnowproc program", 0, WHATSW) \
    X("nowhatnowproc", 0, NWHATSW) \
    X("dashstuffing", 0, BITSTUFFSW)          /* interface to mhl */ \
    X("nodashstuffing", 0, NBITSTUFFSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("file file", 4, FILESW) \
    X("build", 5, BILDSW)                     /* interface from mhe */ \
    X("from address", 0, FROMSW) \
    X("to address", 0, TOSW) \
    X("cc address", 0, CCSW) \
    X("subject text", 0, SUBJECTSW) \
    X("fcc mailbox", 0, FCCSW) \
    X("width columns", 0, WIDTHSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(FORW);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(FORW, switches);
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


int
main (int argc, char **argv)
{
    bool anot = false;
    bool inplace = true;
    bool mime = false;
    int issue = 0, volume = 0, dashstuff = 0;
    bool nedit = false;
    bool nwhat = false;
    int i, in;
    int out, isdf = 0, msgnum = 0;
    int outputlinelen = OUTPUTLINELEN;
    int dat[5];
    char *cp, *cwd, *maildir, *dfolder = NULL;
    char *dmsg = NULL, *digest = NULL, *ed = NULL;
    char *file = NULL, *filter = NULL, *folder = NULL, *fwdmsg = NULL;
    char *from = NULL, *to = NULL, *cc = NULL, *subject = NULL, *fcc = NULL;
    char *form = NULL, buf[BUFSIZ];
    char **argp, **arguments;
    struct stat st;
    struct msgs_array msgs = { 0, 0, NULL };
    bool buildsw = false;

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
		    snprintf (buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

		case ANNOSW: 
		    anot = true;
		    continue;
		case NANNOSW: 
		    anot = false;
		    continue;

		case EDITRSW: 
		    if (!(ed = *argp++) || *ed == '-')
			die("missing argument to %s", argp[-2]);
		    nedit = false;
		    continue;
		case NEDITSW:
		    nedit = true;
		    continue;

		case WHATSW: 
		    if (!(whatnowproc = *argp++) || *whatnowproc == '-')
			die("missing argument to %s", argp[-2]);
		    nwhat = false;
		    continue;
		case BILDSW:
		    buildsw = true;
		    /* FALLTHRU */
		case NWHATSW: 
		    nwhat = true;
		    continue;

		case FILESW: 
		    if (file)
			die("only one file at a time!");
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    file = path (cp, TFILE);
		    continue;
		case FILTSW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    filter = mh_xstrdup(etcpath(cp));
		    mime = false;
		    continue;
		case FORMSW: 
		    if (!(form = *argp++) || *form == '-')
			die("missing argument to %s", argp[-2]);
		    continue;

		case FRMTSW:
		    filter = mh_xstrdup(etcpath(mhlforward));
		    continue;
		case NFRMTSW:
		    filter = NULL;
		    continue;

		case INPLSW: 
		    inplace = true;
		    continue;
		case NINPLSW: 
		    inplace = false;
		    continue;

		case MIMESW:
		    mime = true;
		    filter = NULL;
		    continue;
		case NMIMESW: 
		    mime = false;
		    continue;

		case DGSTSW: 
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    digest = mh_xstrdup(cp);
		    mime = false;
		    continue;
		case ISSUESW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    if ((issue = atoi (cp)) < 1)
			die("bad argument %s %s", argp[-2], cp);
		    continue;
		case VOLUMSW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    if ((volume = atoi (cp)) < 1)
			die("bad argument %s %s", argp[-2], cp);
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
		    if (dmsg)
			die("only one draft message at a time!");
		    if (!(dmsg = *argp++) || *dmsg == '-')
			die("missing argument to %s", argp[-2]);
		    continue;
		case NDFLDSW: 
		    dfolder = NULL;
		    isdf = NOTOK;
		    continue;

		case BITSTUFFSW: 
		    dashstuff = 1;	/* ternary logic */
		    continue;
		case NBITSTUFFSW: 
		    dashstuff = -1;	/* ternary logic */
		    continue;

		case FROMSW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    from = addlist(from, cp);
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
		case FCCSW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    fcc = addlist(fcc, cp);
		    continue;
		case SUBJECTSW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    subject = mh_xstrdup(cp);
		    continue;

		case WIDTHSW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    if ((outputlinelen = atoi(cp)) < 10)
			die("impossible width %d", outputlinelen);
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		die("only one folder at a time!");
            folder = pluspath (cp);
	} else {
	    app_msgarg(&msgs, cp);
	}
    }

    cwd = mh_xstrdup(pwd ());

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (file && (msgs.size || folder))
	die("can't mix files and folders/msgs");

try_it_again:

    strncpy (drft, buildsw ? m_maildir ("draft")
			  : m_draft (dfolder, NULL, NOUSE, &isdf), sizeof(drft));

    /* Check if a draft already exists */
    if (!buildsw && stat (drft, &st) != NOTOK) {
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
	 * Forwarding a file.
         */
	anot = false;	/* don't want to annotate a file */
    } else {
	/*
	 * Forwarding a message.
	 */
	if (!msgs.size)
	    app_msgarg(&msgs, "cur");
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

	seq_setprev (mp);	/* set the previous sequence */

	/*
	 * Find the first message in our set and use it as the input
	 * for the component scanner
	 */

	for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	    if (is_selected (mp, msgnum)) {
		fwdmsg = mh_xstrdup(m_name(msgnum));
		break;
	    }

	if (! fwdmsg)
	    die("Unable to find input message");
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
	if (volume == 0) {
	    snprintf (buf, sizeof(buf), VFORMAT, digest);
	    if ((cp = context_find (buf)) == NULL || (volume = atoi (cp)) <= 0)
		volume = 1;
	}
	if (!form)
	    form = digestcomps;
    } else {
    	if (!form)
    	    form = forwcomps;
    }

    dat[0] = digest ? issue : msgnum;
    dat[1] = volume;
    dat[2] = 0;
    dat[3] = outputlinelen;
    dat[4] = 0;


    in = build_form (form, digest, dat, from, to, cc, fcc, subject,
    		     file ? file : fwdmsg);

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
	    context_replace (buf, mh_xstrdup(m_str(issue)));
	    snprintf (buf, sizeof(buf), VFORMAT, digest);
	    context_replace (buf, mh_xstrdup(m_str(volume)));
	}

	context_replace (pfolder, folder);	/* update current folder   */
	seq_setcur (mp, mp->lowsel);		/* update current message  */
	seq_save (mp);				/* synchronize sequences   */
	context_save ();			/* save the context file   */
    }

    if (nwhat)
	done (0);
    what_now (ed, nedit, NOUSE, drft, NULL, 0, mp,
	anot ? "Forwarded" : NULL, inplace, cwd, 0);
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
    int msgnum, pd[2];
    char buf1[BUFSIZ];
    char buf2[BUFSIZ];
    char *program;
    struct msgs_array vec = { 0, 0, NULL };

    if (pipe (pd) == NOTOK)
	adios ("pipe", "unable to create");

    argsplit_msgarg(&vec, mhlproc, &program);

    child_id = fork();
    switch (child_id) {
	case NOTOK: 
	    adios ("fork", "unable to");

	case OK: 
	    close (pd[0]);
	    dup2 (pd[1], 1);
	    close (pd[1]);

	    app_msgarg(&vec, "-forwall");
	    app_msgarg(&vec, "-form");
	    app_msgarg(&vec, filter);

	    if (digest) {
		app_msgarg(&vec, "-digest");
		app_msgarg(&vec, digest);
		app_msgarg(&vec, "-issue");
		snprintf (buf1, sizeof(buf1), "%d", issue);
		app_msgarg(&vec, buf1);
		app_msgarg(&vec, "-volume");
		snprintf (buf2, sizeof(buf2), "%d", volume);
		app_msgarg(&vec, buf2);
	    }

	    /*
	     * Are we dashstuffing (quoting) the lines that begin
	     * with `-'.  We use the mhl default (don't add any flag)
	     * unless the user has specified a specific flag.
	     */
	    if (dashstuff > 0)
		app_msgarg(&vec, "-dashstuffing");
	    else if (dashstuff < 0)
		app_msgarg(&vec, "-nodashstuffing");

	    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
		if (is_selected (mp, msgnum))
		    app_msgarg(&vec, mh_xstrdup(m_name (msgnum)));
	    }

	    app_msgarg(&vec, NULL);

	    execvp (program, vec.msgs);
	    fprintf (stderr, "unable to exec ");
	    perror (mhlproc);
	    _exit(1);

	default: 
	    close (pd[1]);
	    cpydata (pd[0], out, vec.msgs[0], file);
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
    char *bp, *msgnam;
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
			PLURALS(mp->numsel));
		} else {
		    snprintf (bp, buflen, " Message %d", msgcnt);
		}
		len = strlen (bp);
		bp += len;
		buflen -= len;

		strncpy (bp, "\n\n", buflen);
	    }
	    if (write (out, buffer, strlen (buffer)) < 0) {
		advise (drft, "write");
	    }

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
	snprintf (buffer, sizeof(buffer), "\n------- End of Forwarded Message%s\n",
		PLURALS(mp->numsel));
    }
    if (write (out, buffer, strlen (buffer)) < 0) {
	advise (drft, "write");
    }

    if (digest) {
	snprintf (buffer, sizeof(buffer), "End of %s Digest [Volume %d Issue %d]\n",
		digest, volume, issue);
	i = strlen (buffer);
	for (bp = buffer + i; i > 1; i--)
	    *bp++ = '*';
	*bp++ = '\n';
	*bp = 0;
	if (write (out, buffer, strlen (buffer)) < 0) {
	    advise (drft, "write");
	}
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
	PLURALS(mp->numsel), mp->foldpath);
    if (write (out, buffer, strlen (buffer)) < 0) {
	advise (drft, "write");
    }
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	if (is_selected (mp, msgnum)) {
	    snprintf (buffer, sizeof(buffer), " %s", m_name (msgnum));
	    if (write (out, buffer, strlen (buffer)) < 0) {
		advise (drft, "write");
	    }
	}
    if (write (out, "\n", 1) < 0) {
	advise (drft, "write newline");
    }
}
