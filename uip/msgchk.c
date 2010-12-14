
/*
 * msgchk.c -- check for mail
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/mts.h>
#include <h/tws.h>
#include <pwd.h>

#ifdef POP
# include <h/popsbr.h>
#endif

#ifndef	POP
# define POPminc(a) (a)
#else
# define POPminc(a)  0
#endif

#ifndef CYRUS_SASL
# define SASLminc(a) (a)
#else
# define SASLminc(a)  0
#endif

static struct swit switches[] = {
#define	DATESW                   0
    { "date", 0 },
#define	NDATESW                  1
    { "nodate", 0 },
#define	NOTESW                   2
    { "notify type", 0 },
#define	NNOTESW                  3
    { "nonotify type", 0 },
#define	HOSTSW                   4
    { "host hostname", POPminc (-4) },
#define	USERSW                   5
    { "user username", POPminc (-4) },
#define PORTSW			 6
    { "port name/number", POPminc(-4) },
#define VERSIONSW                7
    { "version", 0 },
#define	HELPSW                   8
    { "help", 0 },
#define SNOOPSW                  9
    { "snoop", -5 },
#define SASLSW                  10
    { "sasl", SASLminc(-4) },
#define SASLMECHSW		11
    { "saslmech", SASLminc(-5) },
#define PROXYSW 		12
    { "proxy command", POPminc(-5) },
    { NULL, 0 }
};

/*
 * Maximum numbers of users we can check (plus
 * one for the NULL vector at the end).
 */
#define MAXVEC  51

#define	NT_NONE	0x0
#define	NT_MAIL	0x1
#define	NT_NMAI	0x2
#define	NT_ALL	(NT_MAIL | NT_NMAI)

#define	NONEOK	0x0
#define	UUCPOLD	0x1
#define	UUCPNEW	0x2
#define	UUCPOK	(UUCPOLD | UUCPNEW)
#define	MMDFOLD	0x4
#define	MMDFNEW	0x8
#define	MMDFOK	(MMDFOLD | MMDFNEW)


/*
 * static prototypes
 */
static int donote (char *, int);
static int checkmail (char *, char *, int, int, int);

#ifdef POP
static int remotemail (char *, char *, char *, char *, int, int, int, int,
		       char *);
#endif


int
main (int argc, char **argv)
{
    int datesw = 1, notifysw = NT_ALL;
    int status = 0, sasl = 0;
    int snoop = 0, vecp = 0;
    uid_t uid;
    char *cp, *host = NULL, *port = NULL, *user, *proxy = NULL; 
    char buf[BUFSIZ], *saslmech = NULL; 
    char **argp, **arguments, *vec[MAXVEC];
    struct passwd *pw;

#ifdef HESIOD
    struct hes_postoffice *po;
    char *tmphost;
#endif

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    mts_init (invo_name);
    uid = getuid ();
    user = getusername();

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

#ifdef POP
    if ((cp = getenv ("MHPOPDEBUG")) && *cp)
	snoop++;
#endif

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [switches] [users ...]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case DATESW:
		    datesw++;
		    continue;
		case NDATESW:
		    datesw = 0;
		    continue;

		case NOTESW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    notifysw |= donote (cp, 1);
		    continue;
		case NNOTESW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    notifysw &= ~donote (cp, 0);
		    continue;

		case HOSTSW: 
		    if (!(host = *argp++) || *host == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case PORTSW:
		    if (!(port = *argp++) || *port == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		continue;

		case USERSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if (vecp >= MAXVEC-1)
			adios (NULL, "you can only check %d users at a time", MAXVEC-1);
		    else
			vec[vecp++] = cp;
		    continue;

		case SNOOPSW:
		    snoop++;
		    continue;

		case SASLSW:
		    sasl++;
		    continue;
		
		case SASLMECHSW:
		    if (!(saslmech = *argp++) || *saslmech == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case PROXYSW:
		    if (!(proxy = *argp++) || *proxy == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
	    }
	}
	if (vecp >= MAXVEC-1)
	    adios (NULL, "you can only check %d users at a time", MAXVEC-1);
	else
	    vec[vecp++] = cp;
    }

#ifdef POP
    /*
     * If -host is not specified by user
     */
    if (!host || !*host) {
	/*
	 * If "pophost" is specified in mts.conf,
	 * use it as default value.
	 */
	if (pophost && *pophost)
	    host = pophost;
    }
    if (!host || !*host)
	host = NULL;
#endif /* POP */

    if (vecp != 0)
	vec[vecp] = NULL;

#ifdef POP
    if (host) {
	if (vecp == 0) {
	    status = remotemail (host, port, user, proxy, notifysw, 1,
				 snoop, sasl, saslmech);
	} else {
	    for (vecp = 0; vec[vecp]; vecp++)
		status += remotemail (host, port, vec[vecp], proxy, notifysw, 0,
				      snoop, sasl, saslmech);
	}
    } else {
#endif /* POP */

    if (vecp == 0) {
	char *home;

        /* Not sure this check makes sense... */
	if (!geteuid() || NULL == (home = getenv("HOME"))) {
	    pw = getpwnam (user);
	    if (pw == NULL)
		adios (NULL, "unable to get information about user");
	    home = pw->pw_dir;
	}
	status = checkmail (user, home, datesw, notifysw, 1);
    } else {
	for (vecp = 0; vec[vecp]; vecp++) {
	    if ((pw = getpwnam (vec[vecp])))
		status += checkmail (pw->pw_name, pw->pw_dir, datesw, notifysw, 0);
	    else
		advise (NULL, "no such user as %s", vec[vecp]);
	}
    }
#ifdef POP
    }		/* host == NULL */
#endif

    done (status);
    return 1;
}


static struct swit ntswitches[] = {
#define	NALLSW     0
    { "all", 0 },
#define	NMAISW     1
    { "mail", 0 },
#define	NNMAISW	   2
    { "nomail", 0 },
    { NULL, 0 }
};


static int
donote (char *cp, int ntflag)
{
    switch (smatch (cp, ntswitches)) {
	case AMBIGSW: 
	    ambigsw (cp, ntswitches);
	    done (1);
	case UNKWNSW: 
	    adios (NULL, "-%snotify %s unknown", ntflag ? "" : "no", cp);

	case NALLSW: 
	    return NT_ALL;
	case NMAISW: 
	    return NT_MAIL;
	case NNMAISW: 
	    return NT_NMAI;
    }

    return 0; /* Before 1999-07-15, garbage was returned if control got here. */
}


static int
checkmail (char *user, char *home, int datesw, int notifysw, int personal)
{
    int mf, status;
    char buffer[BUFSIZ];
    struct stat st;

    snprintf (buffer, sizeof(buffer), "%s/%s", mmdfldir[0] ? mmdfldir : home, mmdflfil[0] ? mmdflfil : user);
    if (datesw) {
	st.st_size = 0;
	st.st_atime = st.st_mtime = 0;
    }
    mf = (stat (buffer, &st) == NOTOK || st.st_size == 0) ? NONEOK
	: st.st_atime <= st.st_mtime ? MMDFNEW : MMDFOLD;

    if ((mf & UUCPOK) || (mf & MMDFOK)) {
	if (notifysw & NT_MAIL) {
	    printf (personal ? "You have " : "%s has ", user);
	    if (mf & UUCPOK)
		printf ("%s old-style bell", mf & UUCPOLD ? "old" : "new");
	    if ((mf & UUCPOK) && (mf & MMDFOK))
		printf (" and ");
	    if (mf & MMDFOK)
		printf ("%s%s", mf & MMDFOLD ? "old" : "new",
			mf & UUCPOK ? " Internet" : "");
	    printf (" mail waiting");
	} else {
	    notifysw = 0;
	}
	status = 0;
    }
    else {
	if (notifysw & NT_NMAI)
	    printf (personal ? "You don't %s%s" : "%s doesn't %s",
		    personal ? "" : user, "have any mail waiting");
	else
	    notifysw = 0;

	status = 1;
    }

    if (notifysw)
	if (datesw && st.st_atime)
	    printf ("; last read on %s", dtime (&st.st_atime, 1));
    if (notifysw)
	printf ("\n");

    return status;
}


#ifdef POP
extern char response[];

static int
remotemail (char *host, char *port, char *user, char *proxy, int notifysw,
	    int personal, int snoop, int sasl, char *saslmech)
{
    int nmsgs, nbytes, status;
    char *pass = NULL;

    if (user == NULL)
	user = getusername ();
    if (sasl)
	pass = getusername ();
    else
	ruserpass (host, &user, &pass);

    /* open the POP connection */
    if (pop_init (host, user, port, pass, proxy, snoop, sasl, saslmech) == NOTOK
	    || pop_stat (&nmsgs, &nbytes) == NOTOK	/* check for messages  */
	    || pop_quit () == NOTOK) {			/* quit POP connection */
	advise (NULL, "%s", response);
	return 1;
    }

    if (nmsgs) {
	if (notifysw & NT_MAIL) {
	    printf (personal ? "You have " : "%s has ", user);
	    printf ("%d message%s (%d bytes)",
		    nmsgs, nmsgs != 1 ? "s" : "", nbytes);
	}
	else
	    notifysw = 0;

	status = 0;
    } else {
	if (notifysw & NT_NMAI)
	    printf (personal ? "You don't %s%s" : "%s doesn't %s",
		    personal ? "" : user, "have any mail waiting");
	else
	    notifysw = 0;
	status = 1;
    }
    if (notifysw)
	printf (" on %s\n", host);

    return status;
}
#endif /* POP */
