/* msgchk.c -- check for mail
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/ambigsw.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/error.h"
#include "h/mts.h"
#include "h/tws.h"
#include "h/done.h"
#include "h/utils.h"
#include <pwd.h>

#include "h/popsbr.h"

#ifndef CYRUS_SASL
# define SASLminc(a) (a)
#else
# define SASLminc(a)  0
#endif

#ifndef TLS_SUPPORT
# define TLSminc(a) (a)
#else
# define TLSminc(a)  0
#endif

#define MSGCHK_SWITCHES \
    X("date", 0, DATESW) \
    X("nodate", 0, NDATESW) \
    X("notify type", 0, NOTESW) \
    X("nonotify type", 0, NNOTESW) \
    X("host hostname", 0, HOSTSW) \
    X("user username", 0, USERSW) \
    X("port name/number", 0, PORTSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("snoop", 0, SNOOPSW) \
    X("sasl", SASLminc(4), SASLSW) \
    X("nosasl", SASLminc(6), NOSASLSW) \
    X("saslmech", SASLminc(5), SASLMECHSW) \
    X("authservice", SASLminc(0), AUTHSERVICESW) \
    X("initialtls", TLSminc(-10), INITTLSSW) \
    X("notls", TLSminc(-5), NOTLSSW) \
    X("certverify", TLSminc(-10), CERTVERSW) \
    X("nocertverify", TLSminc(-12), NOCERTVERSW) \
    X("proxy command", 0, PROXYSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MSGCHK);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MSGCHK, switches);
#undef X

/*
 * Maximum numbers of users we can check (plus
 * one for the NULL vector at the end).
 */
#define MAXVEC  51

#define	NT_MAIL	0x1
#define	NT_NMAI	0x2
#define	NT_ALL	(NT_MAIL | NT_NMAI)


/*
 * static prototypes
 */
static int donote (char *, int) PURE;
static int checkmail (char *, char *, int, int, int);
static int remotemail (char *, char *, char *, char *, int, int, int, int,
		       char *, int, const char *);


int
main (int argc, char **argv)
{
    bool datesw = true;
    int notifysw = NT_ALL;
    int status = 0;
    bool sasl = false;
    bool tls = false;
    bool noverify = false;
    bool snoop = false;
    int vecp = 0;
    char *cp, *host = NULL, *port = NULL, *user = NULL, *proxy = NULL;
    char buf[BUFSIZ], *saslmech = NULL, *auth_svc = NULL;
    char **argp, **arguments, *vec[MAXVEC];
    struct passwd *pw;

    if (nmh_init(argv[0], true, true)) { return 1; }

    mts_init ();

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
		    snprintf (buf, sizeof(buf), "%s [switches] [users ...]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

		case DATESW:
		    datesw = true;
		    continue;
		case NDATESW:
		    datesw = false;
		    continue;

		case NOTESW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    notifysw |= donote (cp, 1);
		    continue;
		case NNOTESW:
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    notifysw &= ~donote (cp, 0);
		    continue;

		case HOSTSW: 
		    if (!(host = *argp++) || *host == '-')
			die("missing argument to %s", argp[-2]);
		    continue;

		case PORTSW:
		    if (!(port = *argp++) || *port == '-')
			die("missing argument to %s", argp[-2]);
		continue;

		case USERSW: 
		    if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		    if (vecp >= MAXVEC-1)
			die("you can only check %d users at a time", MAXVEC-1);
                    user = vec[vecp++] = cp;
		    continue;

		case SNOOPSW:
		    snoop = true;
		    continue;

		case SASLSW:
		    sasl = true;
		    continue;
		
		case NOSASLSW:
		    sasl = false;
		    continue;

		case SASLMECHSW:
		    if (!(saslmech = *argp++) || *saslmech == '-')
			die("missing argument to %s", argp[-2]);
		    continue;

		case INITTLSSW:
		    tls = true;
		    continue;

		case NOTLSSW:
		    tls = false;
		    continue;

		case CERTVERSW:
		    noverify = false;
		    continue;

		case NOCERTVERSW:
		    noverify = true;
		    continue;

		case AUTHSERVICESW:
#ifdef OAUTH_SUPPORT
		    if (!(auth_svc = *argp++) || *auth_svc == '-')
			die("missing argument to %s", argp[-2]);
#else
		    die("not built with OAuth support");
#endif
		    continue;

		case PROXYSW:
		    if (!(proxy = *argp++) || *proxy == '-')
			die("missing argument to %s", argp[-2]);
		    continue;
	    }
	}
	if (vecp >= MAXVEC-1)
	    die("you can only check %d users at a time", MAXVEC-1);
        vec[vecp++] = cp;
    }

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

    if (vecp != 0)
	vec[vecp] = NULL;

    if (host) {
	int tlsflag = 0;

	if (tls)
	    tlsflag |= P_INITTLS;

	if (noverify)
	    tlsflag |= P_NOVERIFY;

	if (vecp == 0) {
	    status = remotemail (host, port, user, proxy, notifysw, 1,
				 snoop, sasl, saslmech, tlsflag, auth_svc);
	} else {
	    for (vecp = 0; vec[vecp]; vecp++)
		status += remotemail (host, port, vec[vecp], proxy, notifysw, 0,
				      snoop, sasl, saslmech, tlsflag, auth_svc);
	}
    } else {
	if (user == NULL) user = getusername ();
	if (vecp == 0) {
	    char *home;

	    /* Not sure this check makes sense... */
	    if (!geteuid() || NULL == (home = getenv("HOME"))) {
		pw = getpwnam (user);
		if (pw == NULL)
		    die("unable to get information about user");
		home = pw->pw_dir;
	    }
	    status = checkmail (user, home, datesw, notifysw, 1);
	} else {
	    for (vecp = 0; vec[vecp]; vecp++) {
		if ((pw = getpwnam (vec[vecp])))
		    status += checkmail (pw->pw_name, pw->pw_dir, datesw, notifysw, 0);
		else
		    inform("no such user as %s", vec[vecp]);
	    }
	}
    }		/* host == NULL */

    done (status);
    return 1;
}


#define NOTE_SWITCHES \
    X("all", 0, NALLSW) \
    X("mail", 0, NMAISW) \
    X("nomail", 0, NNMAISW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(NOTE);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(NOTE, ntswitches);
#undef X


static int
donote (char *cp, int ntflag)
{
    switch (smatch (cp, ntswitches)) {
	case AMBIGSW: 
	    ambigsw (cp, ntswitches);
	    done (1);
	case UNKWNSW: 
	    die("-%snotify %s unknown", ntflag ? "" : "no", cp);

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
    char buffer[BUFSIZ];
    snprintf(buffer, sizeof buffer, "%s/%s", *mmdfldir ? mmdfldir : home,
        *mmdflfil ? mmdflfil : user);

    struct stat st;
    bool statok = stat(buffer, &st) != -1;
    bool empty = !statok || st.st_size == 0;

    if ((empty && !(notifysw & NT_NMAI)) ||
        (!empty && !(notifysw & NT_MAIL))) {
        return empty;
    }

    if (empty) {
        if (personal) {
            fputs("You don't have any mail waiting", stdout);
        } else {
            printf("%s doesn't have any mail waiting", user);
        }
    } else {
        char *kind = st.st_mtime < st.st_atime ? "old" : "new";
        if (personal) {
            printf("You have %s mail waiting", kind);
        } else {
            printf("%s has %s mail waiting", user, kind);
        }
    }

    if (datesw && statok && st.st_atime)
        printf("; last read on %s", dtime(&st.st_atime, 1));
    putchar('\n');

    return empty;
}


extern char response[];

static int
remotemail (char *host, char *port, char *user, char *proxy, int notifysw,
	    int personal, int snoop, int sasl, char *saslmech, int tls,
	    const char *auth_svc)
{
    int nmsgs, nbytes;
    bool status;

    if (auth_svc == NULL) {
	if (saslmech  &&  ! strcasecmp(saslmech, "xoauth2")) {
	    die("must specify -authservice with -saslmech xoauth2");
	}
    } else {
	if (user == NULL) {
	    die("must specify -user with -saslmech xoauth2");
	}
    }

    /* open the POP connection */
    if (pop_init (host, port, user, proxy, snoop, sasl, saslmech, tls,
		  auth_svc) == NOTOK
	    || pop_stat (&nmsgs, &nbytes) == NOTOK     /* check for messages  */
	    || pop_quit () == NOTOK) {                 /* quit POP connection */
	inform("%s", response);
	return 1;
    }

    if (nmsgs) {
	if (notifysw & NT_MAIL) {
	    if (personal)
		fputs("You have ", stdout);
	    else
		printf ("%s has ", user);

	    printf ("%d message%s (%d bytes)",
		    nmsgs, PLURALS(nmsgs), nbytes);
	}
	else
	    notifysw = 0;

	status = false;
    } else {
	if (notifysw & NT_NMAI)
	    printf (personal ? "You don't %s%s" : "%s doesn't %s",
		    personal ? "" : user, "have any mail waiting");
	else
	    notifysw = 0;
	status = true;
    }
    if (notifysw)
	printf (" on %s\n", host);

    return status;
}
