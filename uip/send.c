/* send.c -- send a composed message
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include "h/done.h"
#include <h/utils.h>
#ifdef OAUTH_SUPPORT
# include <h/oauth.h>
#endif
#include "sbr/m_maildir.h"
#include "sbr/m_mktemp.h"

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

#define SEND_SWITCHES \
    X("alias aliasfile", 0, ALIASW) \
    X("debug", -5, DEBUGSW) \
    X("draft", 0, DRAFTSW) \
    X("draftfolder +folder", 6, DFOLDSW) \
    X("draftmessage msg", 6, DMSGSW) \
    X("nodraftfolder", 0, NDFLDSW) \
    X("filter filterfile", 0, FILTSW) \
    X("nofilter", 0, NFILTSW) \
    X("format", 0, FRMTSW) \
    X("noformat", 0, NFRMTSW) \
    X("forward", 0, FORWSW) \
    X("noforward", 0, NFORWSW) \
    X("mime", 0, MIMESW) \
    X("nomime", 0, NMIMESW) \
    X("msgid", 0, MSGDSW) \
    X("nomsgid", 0, NMSGDSW) \
    X("push", 0, PUSHSW) \
    X("nopush", 0, NPUSHSW) \
    X("split seconds", 0, SPLITSW) \
    X("unique", -6, UNIQSW) \
    X("nounique", -8, NUNIQSW) \
    X("verbose", 0, VERBSW) \
    X("noverbose", 0, NVERBSW) \
    X("watch", 0, WATCSW) \
    X("nowatch", 0, NWATCSW) \
    X("width columns", 0, WIDTHSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("dashstuffing", -12, BITSTUFFSW) \
    X("nodashstuffing", -14, NBITSTUFFSW) \
    X("client host", -6, CLIESW) \
    X("server host", 6, SERVSW) \
    X("snoop", 5, SNOOPSW) \
    X("sasl", SASLminc(4), SASLSW) \
    X("nosasl", SASLminc(6), NOSASLSW) \
    X("saslmech mechanism", SASLminc(6), SASLMECHSW) \
    X("authservice", SASLminc(0), AUTHSERVICESW) \
    X("user username", SASLminc(-4), USERSW) \
    X("port server-port-name/number", 4, PORTSW) \
    X("tls", TLSminc(-3), TLSSW) \
    X("initialtls", TLSminc(-10), INITTLSSW) \
    X("notls", TLSminc(-5), NTLSSW) \
    X("certverify", TLSminc(-10), CERTVERSW) \
    X("nocertverify", TLSminc(-12), NOCERTVERSW) \
    X("sendmail program", 0, MTSSM) \
    X("mts smtp|sendmail/smtp|sendmail/pipe", 2, MTSSW) \
    X("messageid localname|random", 2, MESSAGEIDSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(SEND);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(SEND, switches);
#undef X

#define USE_SWITCHES \
    X("no", 0, NOSW) \
    X("yes", 0, YESW) \
    X("list", 0, LISTDSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(USE);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(USE, anyl);
#undef X

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
    int msgp = 0, vecp;
    int isdf = 0, mime = 0;
    int msgnum, status;
    char *cp, *dfolder = NULL, *maildir = NULL;
    char buf[BUFSIZ], **ap, **argp, **arguments, *program;
    char *msgs[MAXARGS], **vec;
    const char *user = NULL, *saslmech = NULL;
    struct msgs *mp;
    struct stat st;
    int snoop = 0;
    char *auth_svc = NULL;

    if (nmh_init(argv[0], 1)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    vec = argsplit(postproc, &program, &vecp);

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

    if ((cp = context_find ("credentials"))) {
	/* post doesn't read context so need to pass credentials. */
	vec[vecp++] = "-credentials";
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
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

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

		case SNOOPSW:
                    snoop++;
		    vec[vecp++] = --cp;
		    continue;

		case DEBUGSW: 
		    debugsw++;
		    /* FALLTHRU */
		case NFILTSW: 
		case FRMTSW: 
		case NFRMTSW: 
		case BITSTUFFSW:
		case NBITSTUFFSW:
		case MSGDSW: 
		case NMSGDSW: 
		case WATCSW: 
		case NWATCSW: 
		case SASLSW:
		case NOSASLSW:
		case TLSSW:
		case INITTLSSW:
		case NTLSSW:
		case CERTVERSW:
		case NOCERTVERSW:
		    vec[vecp++] = --cp;
		    continue;

		case USERSW:
		    vec[vecp++] = --cp;
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    vec[vecp++] = cp;
                    user = cp;
		    continue;

		case AUTHSERVICESW:
#ifdef OAUTH_SUPPORT
		    if (!(auth_svc = *argp++) || *auth_svc == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
#else
		    adios (NULL, "not built with OAuth support");
#endif
		    continue;

		case SASLMECHSW:
		    if (!(saslmech = *argp) || *saslmech == '-')
			adios (NULL, "missing argument to %s", argp[-1]);
		    /* FALLTHRU */

		case ALIASW: 
		case FILTSW: 
		case WIDTHSW: 
		case CLIESW: 
		case SERVSW: 
		case PORTSW:
		case MTSSM:
		case MTSSW:
		case MESSAGEIDSW:
		    vec[vecp++] = --cp;
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    vec[vecp++] = cp;
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

	for (ap = brkstring(dp = mh_xstrdup(cp), " ", "\n"); ap && *ap; ap++) {
	    vec[vecp++] = "-alias";
	    vec[vecp++] = *ap;
	}
    }

    if (dfolder == NULL) {
	if (msgp == 0) {
	    msgs[msgp++] = getcpy (m_draft (NULL, NULL, 1, &isdf));
	    if (stat (msgs[0], &st) == NOTOK)
		adios (msgs[0], "unable to stat draft file");
	    cp = concat ("Use \"", msgs[0], "\"? ", NULL);
	    for (status = LISTDSW; status != YESW;) {
		if (!(argp = read_switch_multiword (cp, anyl)))
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
			inform("say what?");
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
	if (!(mp = folder_read (dfolder, 1)))
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
		msgs[msgp++] = mh_xstrdup(m_name (msgnum));
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
	    setenv("SIGNATURE", cp, 1);

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
	    && atoi(cp)
	    && altmsg) {
	vec[vecp++] = "-dist";
	if ((cp = m_mktemp2(altmsg, invo_name, NULL, NULL)) == NULL) {
	    adios(NULL, "unable to create temporary file");
	}
	distfile = mh_xstrdup(cp);
	(void) m_unlink(distfile);
	if (link (altmsg, distfile) == NOTOK) {
	    /* Cygwin with FAT32 filesystem produces EPERM. */
	    if (errno != EXDEV  &&  errno != EPERM
#ifdef EISREMOTE
		    && errno != EISREMOTE
#endif /* EISREMOTE */
		)
		adios (distfile, "unable to link %s to", altmsg);
	    free (distfile);
	    if ((cp = m_mktemp2(NULL, invo_name, NULL, NULL)) == NULL) {
		adios(NULL, "unable to create temporary file in %s",
		      get_temp_dir());
	    }
	    distfile = mh_xstrdup(cp);
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

#ifdef OAUTH_SUPPORT
    if (auth_svc == NULL) {
        if (saslmech  &&  ! strcasecmp(saslmech, "xoauth2")) {
            adios (NULL, "must specify -authservice with -saslmech xoauth2");
        }
    } else {
        if (user == NULL) {
            adios (NULL, "must specify -user with -saslmech xoauth2");
        }
    }
#else
    NMH_UNUSED(auth_svc);
    NMH_UNUSED(user);
    NMH_UNUSED(saslmech);
#endif /* OAUTH_SUPPORT */

    if (altmsg == NULL || stat (altmsg, &st) == NOTOK) {
	st.st_mtime = 0;
	st.st_dev = 0;
	st.st_ino = 0;
    }
    if (pushsw)
	push ();

    status = 0;
    closefds (3);

    for (msgnum = 0; msgnum < msgp; msgnum++) {
        switch (sendsbr (vec, vecp, program, msgs[msgnum], &st, 1, auth_svc)) {
	    case DONE: 
		done (++status);
		/* FALLTHRU */
	    case NOTOK: 
		status++;
		/* FALLTHRU */
	    case OK:
		break;
	}
    }

    context_save ();	/* save the context file */
    done (status);
    return 1;
}
