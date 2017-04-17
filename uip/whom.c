
/*
 * whom.c -- report to whom a message would be sent
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <h/signals.h>

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

#define WHOM_SWITCHES \
    X("alias aliasfile", 0, ALIASW) \
    X("check", 0, CHKSW) \
    X("nocheck", 0, NOCHKSW) \
    X("draft", 0, DRAFTSW) \
    X("draftfolder +folder", 6, DFOLDSW) \
    X("draftmessage msg", 6, DMSGSW) \
    X("nodraftfolder", 0, NDFLDSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("client host", -6, CLIESW) \
    X("server host", 0, SERVSW) \
    X("snoop", 0, SNOOPSW) \
    X("sasl", SASLminc(4), SASLSW) \
    X("saslmech mechanism", SASLminc(-5), SASLMECHSW) \
    X("user username", SASLminc(-4), USERSW) \
    X("port server port name/number", 4, PORTSW) \
    X("tls", TLSminc(-3), TLSSW) \
    X("initialtls", TLSminc(-10), INITTLSSW) \
    X("notls", TLSminc(-5), NTLSSW) \
    X("mts smtp|sendmail/smtp|sendmail/pipe", 0, MTSSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(WHOM);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(WHOM, switches);
#undef X


int
main (int argc, char **argv)
{
    pid_t child_id = OK;
    int i, status, isdf = 0;
    int distsw = 0, vecp = 0;
    char *cp, *dfolder = NULL, *dmsg = NULL;
    char *msg = NULL, **ap, **argp, backup[BUFSIZ];
    char buf[BUFSIZ], **arguments, *vec[MAXARGS];

    if (nmh_init(argv[0], 2)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    vec[vecp++] = invo_name;
    vec[vecp++] = "-whom";
    vec[vecp++] = "-library";
    vec[vecp++] = getcpy (m_maildir (""));

    /* Don't need to feed fileproc or mhlproc to post because
       it doesn't use them when used for whom. */

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [switches] [file]", invo_name);
		    print_help (buf, switches, 1);
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

		case CHKSW: 
		case NOCHKSW: 
		case SNOOPSW:
		case SASLSW:
		case TLSSW:
		case INITTLSSW:
		case NTLSSW:
		    vec[vecp++] = --cp;
		    continue;

		case DRAFTSW:
		    msg = draft;
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

		case ALIASW: 
		case CLIESW: 
		case SERVSW: 
		case USERSW:
		case PORTSW:
		case SASLMECHSW:
		case MTSSW:
		    vec[vecp++] = --cp;
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    vec[vecp++] = cp;
		    continue;
	    }
	}
	if (msg)
	    adios (NULL, "only one draft at a time!");
	else
	    vec[vecp++] = msg = cp;
    }

    /* allow Aliasfile: profile entry */
    if ((cp = context_find ("Aliasfile"))) {
	char *dp = NULL;

	for (ap = brkstring(dp = mh_xstrdup(cp), " ", "\n"); ap && *ap; ap++) {
	    vec[vecp++] = "-alias";
	    vec[vecp++] = *ap;
	}
    }

    if (msg == NULL) {
	    cp  = getcpy (m_draft (dfolder, dmsg, 1, &isdf));
	msg = vec[vecp++] = cp;
    }
    if ((cp = getenv ("mhdist"))
	    && *cp
	    && (distsw = atoi (cp))
	    && (cp = getenv ("mhaltmsg"))
	    && *cp) {
	if (distout (msg, cp, backup) == NOTOK)
	    done (1);
	vec[vecp++] = "-dist";
	distsw++;
    }
    vec[vecp] = NULL;

    closefds (3);

    if (distsw) {
	for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
	    sleep (5);
    }

    switch (distsw ? child_id : OK) {
	case NOTOK:
	    inform("unable to fork, so checking directly...");
	    /* FALLTHRU */
	case OK:
	    execvp (postproc, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (postproc);
	    _exit (-1);

	default:
	    SIGNAL (SIGHUP, SIG_IGN);
	    SIGNAL (SIGINT, SIG_IGN);
	    SIGNAL (SIGQUIT, SIG_IGN);
	    SIGNAL (SIGTERM, SIG_IGN);

	    status = pidwait(child_id, OK);

	    (void) m_unlink (msg);
	    if (rename (backup, msg) == NOTOK)
		adios (msg, "unable to rename %s to", backup);
	    done (status);
    }

    return 0;  /* dead code to satisfy the compiler */
}
