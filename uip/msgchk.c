
/*
 * msgchk.c -- check for mail
 *
 * $Id$
 */

#include <h/mh.h>
#include <zotnet/mts/mts.h>
#include <h/tws.h>
#include <pwd.h>

#ifdef POP
# include <h/popsbr.h>
#endif

#ifdef HESIOD
# include <hesiod.h>
#endif

#ifndef	POP
# define POPminc(a) (a)
#else
# define POPminc(a)  0
#endif

#ifndef	RPOP
# define RPOPminc(a) (a)
#else
# define RPOPminc(a)  0
#endif

#ifndef	APOP
# define APOPminc(a) (a)
#else
# define APOPminc(a)  0
#endif

#ifndef	KPOP
# define KPOPminc(a) (a)
#else
# define KPOPminc(a)  0
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
#define	APOPSW                   6
    { "apop", APOPminc (-4) },
#define	NAPOPSW                  7
    { "noapop", APOPminc (-6) },
#define	RPOPSW                   8
    { "rpop", RPOPminc (-4) },
#define	NRPOPSW                  9
    { "norpop", RPOPminc (-6) },
#define VERSIONSW               10
    { "version", 0 },
#define	HELPSW                  11
    { "help", 0 },
#define SNOOPSW                 12
    { "snoop", -5 },
#define KPOPSW                  13
    { "kpop", KPOPminc (-4) },
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
static int remotemail (char *, char *, int, int, int, int, int);
#endif


int
main (int argc, char **argv)
{
    int datesw = 1, notifysw = NT_ALL;
    int rpop, status = 0;
    int kpop = 0;
    int snoop = 0, vecp = 0;
    uid_t uid;
    char *cp, *host = NULL, *user, buf[BUFSIZ];
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

    rpop = 0;

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
		case USERSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if (vecp >= MAXVEC-1)
			adios (NULL, "you can only check %d users at a time", MAXVEC-1);
		    else
			vec[vecp++] = cp;
		    continue;

		case APOPSW: 
		    rpop = -1;
		    continue;
		case NAPOPSW:
		    rpop = 0;
		    continue;

		case RPOPSW: 
		    rpop = 1;
		    continue;
		case NRPOPSW: 
		    rpop = 0;
		    continue;

		case KPOPSW:
		    kpop = 1;
		    continue;

		case SNOOPSW:
		    snoop++;
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
# ifdef HESIOD
	/*
	 * Scheme is:
	 *        use MAILHOST environment variable if present,
	 *  else try Hesiod.
	 *  If that fails, use the default (if any)
	 *  provided by mts.conf in mts_init()
	 */
	if ((tmphost = getenv("MAILHOST")) != NULL)
	    pophost = tmphost;
	else if ((po = hes_getmailhost(vecp ? vec[0] : user)) != NULL &&
		strcmp(po->po_type, "POP") == 0)
	    pophost = po->po_host;
# endif /* HESIOD */
	/*
	 * If "pophost" is specified in mts.conf,
	 * use it as default value.
	 */
	if (pophost && *pophost)
	    host = pophost;
    }
    if (!host || !*host)
	host = NULL;
    if (!host || rpop <= 0)
	setuid (uid);
#endif /* POP */

    if (vecp != 0)
	vec[vecp] = NULL;

#ifdef POP
    if (host) {
	if ( strcmp( POPSERVICE, "kpop" ) == 0 ) {
	    kpop = 1;
	}
	if (vecp == 0) {
	    status = remotemail (host, user, rpop, kpop, notifysw, 1, snoop);
	} else {
	    for (vecp = 0; vec[vecp]; vecp++)
		status += remotemail (host, vec[vecp], rpop, kpop, notifysw, 0, snoop);
	}
    } else {
#endif /* POP */

    if (vecp == 0) {
	char *home;

	home = (uid = geteuid()) ? home = getenv ("HOME") : NULL;
	if (home == NULL) {
	    pw = getpwnam (user);
	    if (pw == NULL)
		adios (NULL, "unable to get information about user");
	    if (home == NULL)
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

    return done (status);
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
remotemail (char *host, char *user, int rpop, int kpop, int notifysw, int personal, int snoop)
{
    int nmsgs, nbytes, status;
    char *pass = NULL;

    if (user == NULL)
	user = getusername ();
    if (kpop || (rpop > 0))
	pass = getusername ();
    else
	ruserpass (host, &user, &pass);

    /* open the POP connection */
    if (pop_init (host, user, pass, snoop, kpop ? 1 : rpop, kpop) == NOTOK
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
