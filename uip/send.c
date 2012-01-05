
/*
 * send.c -- send a composed message
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>


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

static struct swit switches[] = {
#define	ALIASW                 0
    { "alias aliasfile", 0 },
#define	DEBUGSW                1
    { "debug", -5 },
#define	DRAFTSW                2
    { "draft", 0 },
#define	DFOLDSW                3
    { "draftfolder +folder", 6 },
#define	DMSGSW                 4
    { "draftmessage msg", 6 },
#define	NDFLDSW                5
    { "nodraftfolder", 0 },
#define	FILTSW                 6
    { "filter filterfile", 0 },
#define	NFILTSW                7
    { "nofilter", 0 },
#define	FRMTSW                 8
    { "format", 0 },
#define	NFRMTSW                9
    { "noformat", 0 },
#define	FORWSW                10
    { "forward", 0 },
#define	NFORWSW               11
    { "noforward", 0 },
#define MIMESW                12
    { "mime", 0 },
#define NMIMESW               13
    { "nomime", 0 },
#define	MSGDSW                14
    { "msgid", 0 },
#define	NMSGDSW               15
    { "nomsgid", 0 },
#define	PUSHSW                16
    { "push", 0 },
#define	NPUSHSW               17
    { "nopush", 0 },
#define	SPLITSW               18
    { "split seconds", 0 },
#define	UNIQSW                19
    { "unique", -6 },
#define	NUNIQSW               20
    { "nounique", -8 },
#define	VERBSW                21
    { "verbose", 0 },
#define	NVERBSW               22
    { "noverbose", 0 },
#define	WATCSW                23
    { "watch", 0 },
#define	NWATCSW               24
    { "nowatch", 0 },
#define	WIDTHSW               25
    { "width columns", 0 },
#define VERSIONSW             26
    { "version", 0 },
#define	HELPSW                27
    { "help", 0 },
#define BITSTUFFSW            28
    { "dashstuffing", -12 },
#define NBITSTUFFSW           29
    { "nodashstuffing", -14 },
#define	MAILSW                30
    { "mail", -4 },
#define	SAMLSW                31
    { "saml", -4 },
#define	SENDSW                32
    { "send", -4 },
#define	SOMLSW                33
    { "soml", -4 },
#define	CLIESW                34
    { "client host", -6 },
#define	SERVSW                35
    { "server host", 6 },
#define	SNOOPSW               36
    { "snoop", 5 },
#define SASLSW                37
    { "sasl", SASLminc(4) },
#define SASLMECHSW            38
    { "saslmech mechanism", SASLminc(-5) },
#define USERSW                39
    { "user username", SASLminc(-4) },
#define ATTACHSW              40
    { "attach", 6 },
#define ATTACHFORMATSW        41
    { "attachformat", 7 },
#define PORTSW		      42
    { "port server-port-name/number" , 4 },
#define TLSSW		      43
    { "tls", TLSminc(-3) },
    { NULL, 0 }
};

static struct swit anyl[] = {
#define	NOSW     0
    { "no", 0 },
#define	YESW     1
    { "yes", 0 },
#define	LISTDSW  2
    { "list", 0 },
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


int
main (int argc, char **argv)
{
    int msgp = 0, distsw = 0, vecp = 1;
    int isdf = 0, mime = 0;
    int msgnum, status;
    char *cp, *dfolder = NULL, *maildir = NULL;
    char buf[BUFSIZ], **ap, **argp, **arguments;
    char *msgs[MAXARGS], *vec[MAXARGS];
    struct msgs *mp;
    struct stat st;
    char	*attach = (char *)0;	/* header field name for attachments */
    int attachformat = 0; /* mhbuild format specifier for attachments */
#ifdef UCI
    FILE *fp;
#endif /* UCI */

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

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
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown\n", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [file] [switches]", invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case DRAFTSW: 
		    msgs[msgp++] = draft;
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
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    msgs[msgp++] = cp;
		    continue;
		case NDFLDSW: 
		    dfolder = NULL;
		    isdf = NOTOK;
		    continue;

		case PUSHSW: 
		    pushsw++;
		    continue;
		case NPUSHSW: 
		    pushsw = 0;
		    continue;

		case SPLITSW: 
		    if (!(cp = *argp++) || sscanf (cp, "%d", &splitsw) != 1)
			adios (NULL, "missing argument to %s", argp[-2]);
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

		case MIMESW:
		    mime++;
		    vec[vecp++] = --cp;
		    continue;
		case NMIMESW:
		    mime = 0;
		    vec[vecp++] = --cp;
		    continue;

		case DEBUGSW: 
		    debugsw++;	/* fall */
		case NFILTSW: 
		case FRMTSW: 
		case NFRMTSW: 
		case BITSTUFFSW:
		case NBITSTUFFSW:
		case MSGDSW: 
		case NMSGDSW: 
		case WATCSW: 
		case NWATCSW: 
		case MAILSW: 
		case SAMLSW: 
		case SENDSW: 
		case SOMLSW: 
		case SNOOPSW: 
		case SASLSW:
		case TLSSW:
		    vec[vecp++] = --cp;
		    continue;

		case ALIASW: 
		case FILTSW: 
		case WIDTHSW: 
		case CLIESW: 
		case SERVSW: 
		case SASLMECHSW:
		case USERSW:
		case PORTSW:
		    vec[vecp++] = --cp;
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    vec[vecp++] = cp;
		    continue;
		
		case ATTACHSW:
		    if (!(attach = *argp++) || *attach == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case ATTACHFORMATSW:
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
	} else {
	    msgs[msgp++] = cp;
	}
    }

    /*
     * check for "Aliasfile:" profile entry
     */
    if ((cp = context_find ("Aliasfile"))) {
	char *dp = NULL;

	for (ap = brkstring(dp = getcpy(cp), " ", "\n"); ap && *ap; ap++) {
	    vec[vecp++] = "-alias";
	    vec[vecp++] = *ap;
	}
    }

    if (dfolder == NULL) {
	if (msgp == 0) {
#ifdef WHATNOW
	    if ((cp = getenv ("mhdraft")) && *cp) {
		msgs[msgp++] = cp;
		goto go_to_it;
	    }
#endif /* WHATNOW */
	    msgs[msgp++] = getcpy (m_draft (NULL, NULL, 1, &isdf));
	    if (stat (msgs[0], &st) == NOTOK)
		adios (msgs[0], "unable to stat draft file");
	    cp = concat ("Use \"", msgs[0], "\"? ", NULL);
	    for (status = LISTDSW; status != YESW;) {
		if (!(argp = getans (cp, anyl)))
		    done (1);
		switch (status = smatch (*argp, anyl)) {
		    case NOSW: 
			done (0);
		    case YESW: 
			break;
		    case LISTDSW: 
			showfile (++argp, msgs[0]);
			break;
		    default:
			advise (NULL, "say what?");
			break;
		}
	    }
	} else {
	    for (msgnum = 0; msgnum < msgp; msgnum++)
		msgs[msgnum] = getcpy (m_maildir (msgs[msgnum]));
	}
    } else {
	if (!context_find ("path"))
	    free (path ("./", TFOLDER));

	if (!msgp)
	    msgs[msgp++] = "cur";
	maildir = m_maildir (dfolder);

	if (chdir (maildir) == NOTOK)
	    adios (maildir, "unable to change directory to");

	/* read folder and create message structure */
	if (!(mp = folder_read (dfolder)))
	    adios (NULL, "unable to read folder %s", dfolder);

	/* check for empty folder */
	if (mp->nummsg == 0)
	    adios (NULL, "no messages in %s", dfolder);

	/* parse all the message ranges/sequences and set SELECTED */
	for (msgnum = 0; msgnum < msgp; msgnum++)
	    if (!m_convert (mp, msgs[msgnum]))
		done (1);
	seq_setprev (mp);	/* set the previous-sequence */

	for (msgp = 0, msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	    if (is_selected (mp, msgnum)) {
		msgs[msgp++] = getcpy (m_name (msgnum));
		unset_exists (mp, msgnum);
	    }
	}

	mp->msgflags |= SEQMOD;
	seq_save (mp);
    }

#ifdef WHATNOW
go_to_it:
#endif /* WHATNOW */

    if ((cp = getenv ("SIGNATURE")) == NULL || *cp == 0)
	if ((cp = context_find ("signature")) && *cp)
	    m_putenv ("SIGNATURE", cp);
#ifdef UCI
	else {
	    snprintf (buf, sizeof(buf), "%s/.signature", mypath);
	    if ((fp = fopen (buf, "r")) != NULL
		&& fgets (buf, sizeof buf, fp) != NULL) {
		    fclose (fp);
		    if (cp = strchr (buf, '\n'))
			*cp = 0;
		    m_putenv ("SIGNATURE", buf);
	    }
	}
#endif /* UCI */

    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (stat (msgs[msgnum], &st) == NOTOK)
	    adios (msgs[msgnum], "unable to stat draft file");

    if ((annotext = getenv ("mhannotate")) == NULL || *annotext == 0)
	annotext = NULL;
    if (annotext && ((cp = getenv ("mhinplace")) != NULL && *cp != 0))
	inplace = atoi (cp);
    if ((altmsg = getenv ("mhaltmsg")) == NULL || *altmsg == 0)
	altmsg = NULL;	/* used by dist interface - see below */

    if ((cp = getenv ("mhdist"))
	    && *cp
	    && (distsw = atoi (cp))
	    && altmsg) {
	vec[vecp++] = "-dist";
	distfile = getcpy (m_mktemp2 (altmsg, invo_name, NULL, NULL));
	if (link (altmsg, distfile) == NOTOK) {
	    if (errno != EXDEV
#ifdef EISREMOTE
		    && errno != EISREMOTE
#endif /* EISREMOTE */
		)
		adios (distfile, "unable to link %s to", altmsg);
	    free (distfile);
	    distfile = getcpy (m_mktemp2(NULL, invo_name, NULL, NULL));
	    {
		int in, out;
		struct stat st;

		if ((in = open (altmsg, O_RDONLY)) == NOTOK)
		    adios (altmsg, "unable to open");
		fstat(in, &st);
		if ((out = creat (distfile, (int) st.st_mode & 0777)) == NOTOK)
		    adios (distfile, "unable to write");
		cpydata (in, out, altmsg, distfile);
		close (in);
		close (out);
	    }	
	}
    } else {
	distfile = NULL;
    }

    if (altmsg == NULL || stat (altmsg, &st) == NOTOK) {
	st.st_mtime = 0;
	st.st_dev = 0;
	st.st_ino = 0;
    }
    if (pushsw)
	push ();

    status = 0;
    vec[0] = r1bindex (postproc, '/');
    closefds (3);

    for (msgnum = 0; msgnum < msgp; msgnum++) {
	switch (sendsbr (vec, vecp, msgs[msgnum], &st, 1, attach,
			 attachformat)) {
	    case DONE: 
		done (++status);
	    case NOTOK: 
		status++;	/* fall */
	    case OK:
		break;
	}
    }

    context_save ();	/* save the context file */
    done (status);
    return 1;
}
