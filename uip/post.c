/*
 * post.c -- enter messages into the mail transport system
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/signals.h>
#include <h/addrsbr.h>
#include <h/aliasbr.h>
#include <h/dropsbr.h>
#include <h/mime.h>
#include <h/utils.h>
#include <h/tws.h>
#include <h/mts.h>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#include <mts/smtp/smtp.h>

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

#ifndef OAUTH_SUPPORT
# define OAUTHminc(a)	(a)
#else /* OAUTH_SUPPORT */
# define OAUTHminc(a)	0
#endif /* OAUTH_SUPPORT */

#define FCCS		10	/* max number of fccs allowed */

/* In the following array of structures, the numeric second field of the
   structures (minchars) is apparently used like this:

   -# : Switch can be abbreviated to # characters; switch hidden in -help.
   0  : Switch can't be abbreviated;               switch shown in -help.
   #  : Switch can be abbreviated to # characters; switch shown in -help. */

#define POST_SWITCHES \
    X("alias aliasfile", 0, ALIASW) \
    X("check", -5, CHKSW) /* interface from whom */ \
    X("nocheck", -7, NCHKSW) /* interface from whom */ \
    X("debug", -5, DEBUGSW) \
    X("dist", -4, DISTSW) /* interface from dist */ \
    X("filter filterfile", 0, FILTSW) \
    X("nofilter", 0, NFILTSW) \
    X("format", 0, FRMTSW) \
    X("noformat", 0, NFRMTSW) \
    X("library directory", -7, LIBSW) /* interface from send, whom */ \
    X("mime", 0, MIMESW) \
    X("nomime", 0, NMIMESW) \
    X("msgid", 0, MSGDSW) \
    X("nomsgid", 0, NMSGDSW) \
    X("verbose", 0, VERBSW) \
    X("noverbose", 0, NVERBSW) \
    X("watch", 0, WATCSW) \
    X("nowatch", 0, NWATCSW) \
    X("whom", -4, WHOMSW) /* interface from whom */ \
    X("width columns", 0, WIDTHSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("dashstuffing", -12, BITSTUFFSW) /* should we dashstuff BCC messages? */ \
    X("nodashstuffing", -14, NBITSTUFFSW) \
    X("idanno number", -6, ANNOSW) /* interface from send    */ \
    X("client host", -6, CLIESW) \
    X("server host", 6, SERVSW) /* specify alternate SMTP server */ \
    X("snoop", -5, SNOOPSW) /* snoop the SMTP transaction */ \
    X("partno", -6, PARTSW) \
    X("sasl", SASLminc(4), SASLSW) \
    X("nosasl", SASLminc(6), NOSASLSW) \
    X("saslmech", SASLminc(5), SASLMECHSW) \
    X("user", SASLminc(-4), USERSW) \
    X("port server submission port name/number", 4, PORTSW) \
    X("tls", TLSminc(-3), TLSSW) \
    X("initialtls", TLSminc(-10), INITTLSSW) \
    X("notls", TLSminc(-5), NTLSSW) \
    X("fileproc", -4, FILEPROCSW) \
    X("mhlproc", -3, MHLPROCSW) \
    X("sendmail program", 0, MTSSM) \
    X("mts smtp|sendmail/smtp|sendmail/pipe", 2, MTSSW) \
    X("credentials legacy|file:filename", 0, CREDENTIALSSW) \
    X("messageid localname|random", 2, MESSAGEIDSW) \
    X("authservice auth-service-name", OAUTHminc(-11), AUTHSERVICESW) \
    X("oauthcredfile credential-file", OAUTHminc(-7), OAUTHCREDFILESW) \
    X("oauthclientid client-id", OAUTHminc(-12), OAUTHCLIDSW) \
    X("oauthclientsecret client-secret", OAUTHminc(-12), OAUTHCLSECSW) \
    X("oauthauthendpoint authentication-endpoint", OAUTHminc(-6), OAUTHAUTHENDSW) \
    X("oauthredirect redirect-uri", OAUTHminc(-6), OAUTHREDIRSW) \
    X("oauthtokenendpoint token-endpoint", OAUTHminc(-6), OAUTHTOKENDSW) \
    X("oauthscope scope", OAUTHminc(-6), OAUTHSCOPESW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(POST);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(POST, switches);
#undef X


/*
 * Mapping between command-line switches and profile entries, communicated
 * from 'send'.  We use a service name of 'post' internally.
 */

static struct oauth_profile {
    const char *profname;
    int switchnum;
    const char *value;
} oauthswitches[] = {
    { "oauth-%s-credential-file", OAUTHCREDFILESW, NULL },
    { "oauth-%s-client_id", OAUTHCLIDSW, NULL },
    { "oauth-%s-client_secret", OAUTHCLSECSW, NULL },
    { "oauth-%s-auth_endpoint", OAUTHAUTHENDSW, NULL },
    { "oauth-%s-redirect_uri", OAUTHREDIRSW, NULL },
    { "oauth-%s-token_endpoint", OAUTHTOKENDSW, NULL },
    { "oauth-%s-scope", OAUTHSCOPESW, NULL },
    { NULL, 0, NULL }
};

struct headers {
    char *value;
    unsigned int flags;
    unsigned int set;
};

/*
 * flags for headers->flags
 */
#define	HNOP  0x0000	/* just used to keep .set around		      */
#define	HBAD  0x0001	/* bad header - don't let it through		      */
#define	HADR  0x0002	/* header has an address field			      */
#define	HSUB  0x0004	/* Subject: header				      */
#define	HTRY  0x0008	/* try to send to addrs on header		      */
#define	HBCC  0x0010	/* don't output this header, unless MTS_SENDMAIL_PIPE */
#define	HMNG  0x0020	/* munge this header				      */
#define	HNGR  0x0040	/* no groups allowed in this header		      */
#define	HFCC  0x0080	/* FCC: type header				      */
#define	HNIL  0x0100	/* okay for this header not to have addrs	      */
#define	HIGN  0x0200	/* ignore this header				      */
#define	HDCC  0x0400	/* another undocumented feature			      */
#define HONE  0x0800	/* Only (zero or) one address allowed		      */
#define HEFM  0x1000	/* Envelope-From: header			      */
#define HMIM  0x2000    /* MIME-Version: header                               */
#define HCTE  0x4000    /* Content-Transfer-Encoding: header                  */

/*
 * flags for headers->set
 */
#define	MFRM  0x0001	/* we've seen a From:        */
#define	MDAT  0x0002	/* we've seen a Date:        */
#define	MRFM  0x0004	/* we've seen a Resent-From: */
#define	MVIS  0x0008	/* we've seen sighted addrs  */
#define	MINV  0x0010	/* we've seen blind addrs    */
#define MSND  0x0020	/* we've seen a Sender:      */
#define MRSN  0x0040	/* We've seen a Resent-Sendr:*/
#define MEFM  0x0080	/* We've seen Envelope-From: */
#define MMIM  0x0100    /* We've seen Mime-Version:  */

static struct headers NHeaders[] = {
    { "Return-Path",   HBAD,                0 },
    { "Received",      HBAD,                0 },
    { "Reply-To",      HADR|HNGR,           0 },
    { "From",          HADR|HNGR,           MFRM },
    { "Sender",        HADR|HNGR|HONE,      MSND },
    { "Date",          HBAD,                0 },
    { "Subject",       HSUB,                0 },
    { "To",            HADR|HTRY,           MVIS },
    { "cc",            HADR|HTRY,           MVIS },
    { "Bcc",           HADR|HTRY|HBCC|HNIL, MINV },
    { "Dcc",           HADR|HTRY|HDCC|HNIL, MVIS },	/* sorta cc & bcc combined */
    { "Message-ID",    HBAD,                0 },
    { "Fcc",           HFCC,                0 },
    { "Envelope-From", HADR|HONE|HEFM,      MEFM },
    { "MIME-Version",  HMIM,                MMIM },
    { "Content-Transfer-Encoding",  HCTE,   0 },
    { NULL,            0,                   0 }
};

static struct headers RHeaders[] = {
    { "Resent-Reply-To",   HADR|HNGR,           0 },
    { "Resent-From",       HADR|HNGR,           MRFM },
    { "Resent-Sender",     HADR|HNGR,           MRSN },
    { "Resent-Date",       HBAD,                0 },
    { "Resent-Subject",    HSUB,                0 },
    { "Resent-To",         HADR|HTRY,           MVIS },
    { "Resent-cc",         HADR|HTRY,           MVIS },
    { "Resent-Bcc",        HADR|HTRY|HBCC,      MINV },
    { "Resent-Message-ID", HBAD,                0 },
    { "Resent-Fcc",        HFCC,                0 },
    { "Reply-To",          HADR,                0 },
    { "From",              HADR|HNGR,           MFRM },
    { "Sender",            HADR|HNGR,           MSND },
    { "Date",              HNOP,                MDAT },
    { "To",                HADR|HNIL,           0 },
    { "cc",                HADR|HNIL,           0 },
    { "Bcc",               HADR|HTRY|HBCC|HNIL, 0 },
    { "Fcc",               HIGN,                0 },
    { "Envelope-From",     HADR|HONE|HEFM,      MEFM },
    { "MIME-Version",      HMIM,                MMIM },
    { "Content-Transfer-Encoding",  HCTE,       0 },
    { NULL,                0,                   0 }
};

static short fccind = 0;	/* index into fccfold[] */
static short outputlinelen = OUTPUTLINELEN;

static int pfd = NOTOK;		/* fd to write annotation list to        */
static int recipients = 0;	/* how many people will get a copy       */
static int unkadr = 0;		/* how many of those were unknown        */
static int badadr = 0;		/* number of bad addrs                   */
static int badmsg = 0;		/* message has bad semantics             */
static int verbose = 0;		/* spell it out                          */
static int format = 1;		/* format addresses                      */
static int mime = 0;		/* use MIME-style encapsulations for Bcc */
static int msgid = 0;		/* add msgid                             */
static int debug = 0;		/* debugging post                        */
static int watch = 0;		/* watch the delivery process            */
static int whomsw = 0;		/* we are whom not post                  */
static int checksw = 0;		/* whom -check                           */
static int linepos=0;		/* putadr()'s position on the line       */
static int nameoutput=0;	/* putadr() has output header name       */
static int sasl=0;		/* Use SASL auth for SMTP                */
static char *saslmech=NULL;	/* Force use of particular SASL mech     */
static char *user=NULL;		/* Authenticate as this user             */
static char *port="submission";	/* Name of server port for SMTP submission */
static int tls=-1;		/* Use TLS for encryption		 */
static int fromcount=0;		/* Count of addresses on From: header	 */
static int seensender=0;	/* Have we seen a Sender: header?	 */

static unsigned msgflags = 0;	/* what we've seen */

#define	NORMAL 0
#define	RESENT 1
static int msgstate = NORMAL;

static time_t tclock = 0;	/* the time we started (more or less) */

static SIGNAL_HANDLER hstat, istat, qstat, tstat;

static char tmpfil[BUFSIZ];
static char bccfil[BUFSIZ];

static char from[BUFSIZ];	/* my network address            */
static char sender[BUFSIZ];	/* my Sender: header		 */
static char efrom[BUFSIZ];	/* my Envelope-From: header	 */
static char fullfrom[BUFSIZ];	/* full contents of From header  */
static char *filter = NULL;	/* the filter for BCC'ing        */
static char *subject = NULL;	/* the subject field for BCC'ing */
static char *fccfold[FCCS];	/* foldernames for FCC'ing       */
enum encoding { UNKNOWN = 0, BINARY = 1, SEVENBIT = 7, EIGHTBIT = 8 };
static enum encoding cte = UNKNOWN;

static struct headers  *hdrtab;	/* table for the message we're doing */

static struct mailname localaddrs;		/* local addrs     */
static struct mailname netaddrs;		/* network addrs   */
static struct mailname uuaddrs;			/* uucp addrs      */
static struct mailname tmpaddrs;		/* temporary queue */

static int snoop      = 0;
static char *clientsw = NULL;
static char *serversw = NULL;

static char prefix[] = "----- =_aaaaaaaaaa";

static char *partno = NULL;

/*
 * static prototypes
 */
static void putfmt (char *, char *, int *, FILE *);
static void start_headers (void);
static void finish_headers (FILE *);
static int get_header (char *, struct headers *);
static int putadr (char *, char *, struct mailname *, FILE *, unsigned int);
static void putgrp (char *, char *, FILE *, unsigned int);
static int insert (struct mailname *);
static void pl (void);
static void anno (void);
static int annoaux (struct mailname *);
static void insert_fcc (struct headers *, char *);
static void make_bcc_file (int);
static void verify_all_addresses (int, int, char *, int, char *);
static void chkadr (void);
static void sigon (void);
static void sigoff (void);
static void p_refile (char *);
static void fcc (char *, char *);
static void die (char *, char *, ...);
static void post (char *, int, int, int, char *, int, char *);
static void do_text (char *file, int fd);
static void do_an_address (struct mailname *, int);
static void do_addresses (int, int);
static int find_prefix (void);


int
main (int argc, char **argv)
{
    int state, compnum, dashstuff = 0, swnum, oauth_flag = 0;
    int eai = 0; /* use Email Address Internationalization (EAI) (SMTPUTF8) */
    char *cp, *msg = NULL, **argp, **arguments, *envelope;
    char buf[BUFSIZ], name[NAMESZ], *auth_svc = NULL;
    FILE *in, *out;
    m_getfld_state_t gstate = 0;

    if (nmh_init(argv[0], 0 /* use context_foil() */)) { return 1; }

    mts_init ();
    arguments = getarguments (invo_name, argc, argv, 0);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch ((swnum = smatch (++cp, switches))) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [switches] file", invo_name);
		    print_help (buf, switches, 0);
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

		case LIBSW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    /* create a minimal context */
		    if (context_foil (cp) == -1)
			done (1);
		    continue;

		case ALIASW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((state = alias (cp)) != AK_OK)
			adios (NULL, "aliasing error in %s - %s",
				cp, akerror (state));
		    continue;

		case CHKSW: 
		    checksw++;
		    continue;
		case NCHKSW: 
		    checksw = 0;
		    continue;

		case DEBUGSW: 
		    debug++;
		    continue;

		case DISTSW:
		    msgstate = RESENT;
		    continue;

		case FILTSW:
		    if (!(filter = *argp++) || *filter == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    mime = 0;
		    continue;
		case NFILTSW:
		    filter = NULL;
		    continue;
		
		case FRMTSW: 
		    format++;
		    continue;
		case NFRMTSW: 
		    format = 0;
		    continue;

		case BITSTUFFSW:
		    dashstuff = 1;
		    continue;
		case NBITSTUFFSW:
		    dashstuff = -1;
		    continue;

		case MIMESW:
		    mime++;
		    filter = NULL;
		    continue;
		case NMIMESW: 
		    mime = 0;
		    continue;

		case MSGDSW: 
		    msgid++;
		    continue;
		case NMSGDSW: 
		    msgid = 0;
		    continue;

		case VERBSW: 
		    verbose++;
		    continue;
		case NVERBSW: 
		    verbose = 0;
		    continue;

		case WATCSW: 
		    watch++;
		    continue;
		case NWATCSW: 
		    watch = 0;
		    continue;

		case WHOMSW: 
		    whomsw++;
		    continue;

		case WIDTHSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((outputlinelen = atoi (cp)) < 10)
			adios (NULL, "impossible width %d", outputlinelen);
		    continue;

		case ANNOSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((pfd = atoi (cp)) <= 2)
			adios (NULL, "bad argument %s %s", argp[-2], cp);
		    continue;

		case CLIESW:
		    if (!(clientsw = *argp++) || *clientsw == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case SERVSW:
		    if (!(serversw = *argp++) || *serversw == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case SNOOPSW:
		    snoop++;
		    continue;

		case PARTSW:
		    if (!(partno = *argp++) || *partno == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case SASLSW:
		    sasl++;
		    continue;

		case NOSASLSW:
		    sasl = 0;
		    continue;

		case SASLMECHSW:
		    if (!(saslmech = *argp++) || *saslmech == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case AUTHSERVICESW:
		    if (!(auth_svc = *argp++) || *auth_svc == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    oauth_flag++;
		    continue;

		case OAUTHCREDFILESW:
		case OAUTHCLIDSW:
		case OAUTHCLSECSW:
		case OAUTHAUTHENDSW:
		case OAUTHREDIRSW:
		case OAUTHTOKENDSW:
		case OAUTHSCOPESW:
		{
		    int i;

		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);

		    for (i = 0; oauthswitches[i].profname != NULL; i++) {
			if (oauthswitches[i].switchnum == swnum) {
			    oauthswitches[i].value = cp;
			    break;
			}
		    }

		    if (oauthswitches[i].profname == NULL)
			adios (NULL, "internal error: cannot map switch %s "
			       "to profile entry", argp[-2]);

		    oauth_flag++;
		    continue;
		}

		case USERSW:
		    if (!(user = *argp++) || *user == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case PORTSW:
		    if (!(port = *argp++) || *port == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case TLSSW:
		    tls = 1;
		    continue;

		case INITTLSSW:
		    tls = 2;
		    continue;

		case NTLSSW:
		    tls = 0;
		    continue;

		case FILEPROCSW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    fileproc = cp;
		    continue;

		case MHLPROCSW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    mhlproc = cp;
		    continue;

		case MTSSM:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
                    sendmail = cp;
		    continue;

		case MTSSW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
                    save_mts_method (cp);
		    continue;

		case CREDENTIALSSW: {
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    add_profile_entry ("credentials", cp);
		    continue;
		}

		case MESSAGEIDSW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
                    if (save_message_id_style (cp) != 0)
			adios (NULL, "unsupported messageid \"%s\"", cp);
		    continue;
	    }
	}
	if (msg)
	    adios (NULL, "only one message at a time!");
	else
	    msg = cp;
    }

    alias (AliasFile);

    if (!msg)
	adios (NULL, "usage: %s [switches] file", invo_name);

    if (outputlinelen < 10)
	adios (NULL, "impossible width %d", outputlinelen);

    if ((in = fopen (msg, "r")) == NULL)
	adios (msg, "unable to open");

    start_headers ();
    if (debug) {
	verbose++;
	out = stdout;
    } else {
	if (whomsw) {
	    if ((out = fopen ("/dev/null", "w")) == NULL)
		adios ("/dev/null", "unable to open");
	} else {
	    char *cp = m_mktemp2(NULL, invo_name, NULL, &out);
	    if (cp == NULL) {
		adios(NULL, "unable to create temporary file in %s",
		      get_temp_dir());
	    }
            strncpy(tmpfil, cp, sizeof(tmpfil));
	}
    }

    hdrtab = msgstate == NORMAL ? NHeaders : RHeaders;

    for (compnum = 1;;) {
	int bufsz = sizeof buf;
	switch (state = m_getfld (&gstate, name, buf, &bufsz, in)) {
	    case FLD: 
	    case FLDPLUS: 
                compnum++;
		cp = add (buf, NULL);
		while (state == FLDPLUS) {
		    bufsz = sizeof buf;
		    state = m_getfld (&gstate, name, buf, &bufsz, in);
		    cp = add (buf, cp);
		}
		putfmt (name, cp, &eai, out);
		free (cp);
		continue;

	    case BODY: 
		finish_headers (out);
		if (whomsw)
		    break;
		fprintf (out, "\n%s", buf);
		while (state == BODY) {
		    bufsz = sizeof buf;
		    state = m_getfld (&gstate, name, buf, &bufsz, in);
		    fputs (buf, out);
		}
		break;

	    case FILEEOF: 
		finish_headers (out);
		break;

	    case LENERR: 
	    case FMTERR: 
		adios (NULL, "message format error in component #%d", compnum);

	    default: 
		adios (NULL, "getfld() returned %d", state);
	}
	break;
    }
    m_getfld_state_destroy (&gstate);

    if (pfd != NOTOK)
	anno ();
    fclose (in);

    if (debug) {
	pl ();
	done (0);
    } else {
	fclose (out);
    }

    /*
     * Here's how we decide which address to use as the envelope-from
     * address for SMTP.
     *
     * - If we were given an Envelope-From header, use that.
     * - If we were given a Sender: address, use that.
     * - Otherwise, use the address on the From: line
     */

    if (msgflags & MEFM) {
    	envelope = efrom;
    } else if (seensender) {
    	envelope = sender;
    } else {
    	envelope = from;
    }

    if (tls == -1) {
#ifdef TLS_SUPPORT
	/*
	 * The user didn't specify any of the tls switches.  Try to
	 * help them by implying -initialtls if they're using port 465
	 * (smtps, until IANA revoked that registration in 1998).
	 */
	tls = ! strcmp (port, "465")  ||  ! strcasecmp (port, "smtps")
	    ?  2
	    :  0;
#else  /* ! TLS_SUPPORT */
	tls = 0;
#endif /* ! TLS_SUPPORT */
    }

    /*
     * If we were given any oauth flags, store the appropriate profile
     * entries and make sure an authservice was given (we have to do this
     * here because we aren't guaranteed the authservice will be given on
     * the command line before the other OAuth flags are given).
     */

    if (oauth_flag) {
	int i;
	char sbuf[128];

	if (auth_svc == NULL) {
	    adios(NULL, "No authentication service given with -authservice");
	}

	for (i = 0; oauthswitches[i].profname != NULL; i++) {
		if (oauthswitches[i].value != NULL) {
		    snprintf(sbuf, sizeof(sbuf),
		    oauthswitches[i].profname, auth_svc);
		    sbuf[sizeof(sbuf) - 1] = '\0';
		    add_profile_entry(sbuf, oauthswitches[i].value);
		}
	}
    }

    /* If we are doing a "whom" check */
    if (whomsw) {
	/* This won't work with MTS_SENDMAIL_PIPE. */
        verify_all_addresses (1, eai, envelope, oauth_flag, auth_svc);
	done (0);
    }

    if (msgflags & MINV) {
	make_bcc_file (dashstuff);
	if (msgflags & MVIS) {
	    if (sm_mts != MTS_SENDMAIL_PIPE) {
		/* It would be nice to have support to call
		   verify_all_addresses with MTS_SENDMAIL_PIPE, but
		   that might require running sendmail as root.  Note
		   that spost didn't verify addresses. */
		verify_all_addresses (verbose, eai, envelope, oauth_flag,
                                      auth_svc);
	    }
	    post (tmpfil, 0, verbose, eai, envelope, oauth_flag, auth_svc);
	}
	post (bccfil, 1, verbose, eai, envelope, oauth_flag, auth_svc);
	(void) m_unlink (bccfil);
    } else {
	post (tmpfil, 0, isatty (1), eai, envelope, oauth_flag, auth_svc);
    }

    p_refile (tmpfil);
    (void) m_unlink (tmpfil);

    if (verbose) {
    	if (partno)
	    printf ("Partial Message #%s Processed\n", partno);
	else
	    printf ("Message Processed\n");
    }

    done (0);
    return 1;
}


/*
 * DRAFT GENERATION
 */

static void
putfmt (char *name, char *str, int *eai, FILE *out)
{
    int count, grp, i, keep;
    char *cp, *pp, *qp;
    char namep[BUFSIZ], error[BUFSIZ];
    struct mailname *mp = NULL, *np = NULL;
    struct headers *hdr;

    while (*str == ' ' || *str == '\t')
	str++;

    if (msgstate == NORMAL && uprf (name, "resent")) {
	advise (NULL, "illegal header line -- %s:", name);
	badmsg++;
	return;
    }

    if (! *eai) {
        /* Check each header field value to see if it has any 8-bit characters.
           If it does, enable EAI support. */
        if (contains8bit(str, NULL)) {
            if (verbose) {
                printf ("EAI/SMTPUTF8 enabled\n");
            }

            /* Enable SMTPUTF8. */
            *eai = 1;

            /* Enable passing of utf-8 setting to getname()/getadrx(). */
            enable_eai();
        }
    }

    if ((i = get_header (name, hdrtab)) == NOTOK) {
        if (strncasecmp (name, "nmh-", 4)) {
	    fprintf (out, "%s: %s", name, str);
	} else {
	    /* Filter out all Nmh-* headers, because Norm asked.  They
	       should never have reached this point.  Warn about any
	       that are non-empty. */
	    if (strcmp (str, "\n")) {
		char *newline = strchr (str, '\n');
		if (newline) *newline = '\0';
		if (! whomsw) {
		    advise (NULL, "ignoring header line -- %s: %s", name, str);
		}
	    }
	}

	return;
    }

    hdr = &hdrtab[i];
    if (hdr->flags & HIGN) {
	return;
    }
    if (hdr->flags & HBAD) {
	advise (NULL, "illegal header line -- %s:", name);
	badmsg++;
	return;
    }
    msgflags |= (hdr->set & ~(MVIS | MINV));

    if (hdr->flags & HSUB)
	subject = subject ? add (str, add ("\t", subject)) : mh_xstrdup(str);
    if (hdr->flags & HFCC) {
	if ((cp = strrchr(str, '\n')))
	    *cp = 0;
	for (pp = str; (cp = strchr(pp, ',')); pp = cp) {
	    *cp++ = 0;
	    insert_fcc (hdr, pp);
	}
	insert_fcc (hdr, pp);
	return;
    }
    if (hdr->flags & HCTE) {
        if (strncasecmp (str, "7bit", 4) == 0) {
            cte = SEVENBIT;
        } else if (strncasecmp (str, "8bit", 4) == 0) {
            cte = EIGHTBIT;
        } else if (strncasecmp (str, "binary", 6) == 0) {
            cte = BINARY;
        }
    }
    if (!(hdr->flags & HADR)) {
	fprintf (out, "%s: %s", name, str);
	return;
    }

    tmpaddrs.m_next = NULL;

    for (count = 0; (cp = getname (str)); count++) {
	if ((mp = getm (cp, NULL, 0, error, sizeof(error)))) {
	    if (tmpaddrs.m_next)
		np->m_next = mp;
	    else
		tmpaddrs.m_next = mp;
	    np = mp;
	}
	else {
	    admonish(cp, "%s", error);
	    if (hdr->flags & HTRY)
		badadr++;
	    else
		badmsg++;
	}
    }

    if (count < 1) {
	if (hdr->flags & HNIL)
	    fprintf (out, "%s: %s", name, str);
	else {
	    /*
	     * Sender (or Resent-Sender) can have only one address
	     */
	    if ((msgstate == RESENT) ? (hdr->set & MRSN)
					: (hdr->set & MSND)) {
		advise (NULL, "%s: field requires one address", name);
		badmsg++;
	    }
#ifdef notdef
	    advise (NULL, "%s: field requires at least one address", name);
	    badmsg++;
#endif /* notdef */
	}
	return;
    }

    if (count > 1 && (hdr->flags & HONE)) {
	advise (NULL, "%s: field only permits one address", name);
	badmsg++;
	return;
    }

    nameoutput = linepos = 0;
    snprintf (namep, sizeof(namep), "%s%s",
		(hdr->flags & HMNG) ? "Original-" : "", name);

    for (grp = 0, mp = tmpaddrs.m_next; mp; mp = np)
	if (mp->m_nohost) {	/* also used to test (hdr->flags & HTRY) */
	    /* The address doesn't include a host, so it might be an alias. */
	    pp = akvalue (mp->m_mbox);  /* do mh alias substitution */
	    qp = akvisible () ? mp->m_mbox : "";
	    np = mp;
	    if (np->m_gname)
		putgrp (namep, np->m_gname, out, hdr->flags);
	    while ((cp = getname (pp))) {
		if (!(mp = getm (cp, NULL, 0, error, sizeof(error)))) {
		    admonish(cp, "%s", error);
		    badadr++;
		    continue;
		}

		/*
		 * If it's a From: or Resent-From: header, save the address
		 * for later possible use (as the envelope address for SMTP)
		 */

		if ((msgstate == RESENT) ? (hdr->set & MRFM)
						: (hdr->set & MFRM)) {
		    strncpy(from, auxformat(mp, 0), sizeof(from) - 1);
		    from[sizeof(from) - 1] = '\0';
		    fromcount = count;
		}

		/*
		 * Also save the Sender: or Resent-Sender: header as well
		 */

		if ((msgstate == RESENT) ? (hdr->set & MRSN)
						: (hdr->set & MSND)) {
		    strncpy(sender, auxformat(mp, 0), sizeof(sender) - 1);
		    sender[sizeof(sender) - 1] = '\0';
		    seensender++;
		}

		/*
		 * ALSO ... save Envelope-From
		 */

		if (hdr->set & MEFM) {
		    strncpy(efrom, auxformat(mp, 0), sizeof(efrom) - 1);
		    efrom[sizeof(efrom) - 1] = '\0';
		}

		if (hdr->flags & HBCC)
		    mp->m_bcc++;
		if (np->m_ingrp)
		    mp->m_ingrp = np->m_ingrp;
		else
		    if (mp->m_gname)
			putgrp (namep, mp->m_gname, out, hdr->flags);
		if (mp->m_ingrp) {
		    if (sm_mts == MTS_SENDMAIL_PIPE) {
			/* Catch this before sendmail chokes with:
			   "553 List:; syntax illegal for recipient
			    addresses".
			   If we wanted to, we could expand out blind
			   aliases and put them in Bcc:, but then
			   they'd have the Blind-Carbon-Copy
			   indication. */
			adios (NULL,
			       "blind lists not compatible with"
			       " sendmail/pipe");
		    }

		    grp++;
		}
		if (putadr (namep, qp, mp, out, hdr->flags))
		    msgflags |= (hdr->set & (MVIS | MINV));
		else
		    mnfree (mp);
	    }
	    mp = np;
	    np = np->m_next;
	    mnfree (mp);
	}
	else {
	    /* Address includes a host, so no alias substitution is needed. */

	    /*
	     * If it's a From: or Resent-From header, save the address
	     * for later possible use (as the envelope address for SMTP)
	     */

	    if ((msgstate == RESENT) ? (hdr->set & MRFM)
					    : (hdr->set & MFRM)) {
		strncpy(from, auxformat(mp, 0), sizeof(from) - 1);
		fromcount = count;
	    }

	    /*
	     * Also save the Sender: header as well
	     */

	    if ((msgstate == RESENT) ? (hdr->set & MRSN)
					    : (hdr->set & MSND)) {
	        strncpy(sender, auxformat(mp, 0), sizeof(sender) - 1);
		sender[sizeof(sender) - 1] = '\0';
		seensender++;
	    }

	    /*
	     * ALSO ... save Envelope-From
	     */

	    if (hdr->set & MEFM) {
		strncpy(efrom, auxformat(mp, 0), sizeof(efrom) - 1);
		efrom[sizeof(efrom) - 1] = '\0';
	    }

	    if (hdr->flags & HBCC)
		mp->m_bcc++;
	    if (mp->m_gname)
		putgrp (namep, mp->m_gname, out, hdr->flags);
	    if (mp->m_ingrp)
		grp++;
	    keep = putadr (namep, "", mp, out, hdr->flags);
	    np = mp->m_next;
	    if (keep) {
		mp->m_next = NULL;
		msgflags |= (hdr->set & (MVIS | MINV));
	    }
	    else
		mnfree (mp);
	}

    /*
     * If this is a From:/Resent-From: header, save the full thing for
     * later in case we need it for use when constructing a Bcc draft message
     */

    if ((msgstate == RESENT) ? (hdr->set & MRFM) : (hdr->set & MFRM)) {
    	strncpy(fullfrom, str, sizeof(fullfrom));
	fullfrom[sizeof(fullfrom) - 1] = 0;
	/*
	 * Strip off any trailing newlines
	 */

	while (strlen(fullfrom) > 0 && fullfrom[strlen(fullfrom) - 1] == '\n') {
	    fullfrom[strlen(fullfrom) - 1] = '\0';
	}
    }

    if (grp > 0 && (hdr->flags & HNGR)) {
	advise (NULL, "%s: field does not allow groups", name);
	badmsg++;
    }
    if (linepos) {
	putc ('\n', out);
    }
}


static void
start_headers (void)
{
    time (&tclock);

    /*
     * Probably not necessary, but just in case ...
     */

    from[0] = '\0';
    efrom[0] = '\0';
    sender[0] = '\0';
    fullfrom[0] = '\0';
}


/*
 * Now that we've outputted the header fields in the draft
 * message, we will now output any remaining header fields
 * that we need to add/create.
 */

static void
finish_headers (FILE *out)
{
    switch (msgstate) {
	case NORMAL: 
	    if (!(msgflags & MFRM)) {
		/*
		 * A From: header is now required in the draft.
		 */
		advise (NULL, "message has no From: header");
		advise (NULL, "See default components files for examples");
		badmsg++;
		break;
	    }

	    if (fromcount > 1 && (seensender == 0 && !(msgflags & MEFM))) {
		advise (NULL, "A Sender: or Envelope-From: header is required "
			"with multiple\nFrom: addresses");
		badmsg++;
		break;
	    }

	    if (whomsw)
		break;

	    fprintf (out, "Date: %s\n", dtime (&tclock, 0));
	    if (msgid)
		fprintf (out, "Message-ID: %s\n", message_id (tclock, 0));
	    /*
	     * If we have multiple From: addresses, make sure we have an
	     * Sender: header.  If we don't have one, then generate one
	     * from Envelope-From: (which in this case, cannot be blank)
	     */

	    if (fromcount > 1 && seensender == 0) {
	    	if (efrom[0] == '\0') {
		    advise (NULL, "Envelope-From cannot be blank when there "
		    	    "is multiple From: addresses\nand no Sender: "
			    "header");
		    badmsg++;
		} else {
		    fprintf (out, "Sender: %s\n", efrom);
		}
	    }

	    if (!(msgflags & MVIS))
		fprintf (out, "Bcc: Blind Distribution List: ;\n");
	    break;

	case RESENT: 
	    if (!(msgflags & MDAT)) {
		advise (NULL, "message has no Date: header");
		badmsg++;
	    }
	    if (!(msgflags & MFRM)) {
		advise (NULL, "message has no From: header");
		badmsg++;
	    }
	    if (!(msgflags & MRFM)) {
		advise (NULL, "message has no Resent-From: header");
		advise (NULL, "See default components files for examples");
		badmsg++;
		break;
	    }
	    if (fromcount > 1 && (seensender == 0 && !(msgflags & MEFM))) {
		advise (NULL, "A Resent-Sender: or Envelope-From: header is "
			"required with multiple\nResent-From: addresses");
		badmsg++;
		break;
	    }

	    if (whomsw)
		break;

	    fprintf (out, "Resent-Date: %s\n", dtime (&tclock, 0));
	    if (msgid)
		fprintf (out, "Resent-Message-ID: %s\n",
			 message_id (tclock, 0));
	    /*
	     * If we have multiple Resent-From: addresses, make sure we have an
	     * Resent-Sender: header.  If we don't have one, then generate one
	     * from Envelope-From (which in this case, cannot be blank)
	     */

	    if (fromcount > 1 && seensender == 0) {
	    	if (efrom[0] == '\0') {
		    advise (NULL, "Envelope-From cannot be blank when there "
		    	    "is multiple Resent-From: addresses and no "
			    "Resent-Sender: header");
		    badmsg++;
		} else {
		    fprintf (out, "Resent-Sender: %s\n", efrom);
		}
	    }

	    if (!(msgflags & MVIS))
		fprintf (out, "Resent-Bcc: Blind Re-Distribution List: ;\n");
	    break;
    }

    if (badmsg)
	adios (NULL, "re-format message and try again");
    if (!recipients)
	adios (NULL, "no addressees");
}


static int
get_header (char *header, struct headers *table)
{
    struct headers *h;

    for (h = table; h->value; h++)
	if (!strcasecmp (header ? header : "", h->value ? h->value : ""))
	    return (h - table);

    return NOTOK;
}


static int
putadr (char *name, char *aka, struct mailname *mp, FILE *out, unsigned int flags)
{
    int len;
    char *cp;
    char buffer[BUFSIZ];

    if (mp->m_mbox == NULL || ((flags & HTRY) && !insert (mp)))
	return 0;
    if (sm_mts != MTS_SENDMAIL_PIPE &&
        ((flags & (HBCC | HDCC | HEFM)) || mp->m_ingrp))
	return 1;

    if (!nameoutput) {
	fprintf (out, "%s: ", name);
	linepos += (nameoutput = strlen (name) + 2);
    }

    if (*aka && mp->m_type != UUCPHOST && !mp->m_pers)
	mp->m_pers = mh_xstrdup(aka);
    if (format) {
	if (mp->m_gname) {
	    snprintf (buffer, sizeof(buffer), "%s;", mp->m_gname);
	    cp = buffer;
	} else {
	    cp = adrformat (mp);
	}
    } else {
	cp = mp->m_text;
    }
    len = strlen (cp);

    if (linepos != nameoutput) {
	if (len + linepos + 2 > outputlinelen)
	    fprintf (out, ",\n%*s", linepos = nameoutput, "");
	else {
	    fputs (", ", out);
	    linepos += 2;
	}
    }

    fputs (cp, out);
    linepos += len;

    return (flags & HTRY);
}


static void
putgrp (char *name, char *group, FILE *out, unsigned int flags)
{
    int len;
    char *cp;

    if (sm_mts != MTS_SENDMAIL_PIPE && (flags & HBCC))
	return;

    if (!nameoutput) {
	fprintf (out, "%s: ", name);
	linepos += (nameoutput = strlen (name) + 2);
    }

    cp = concat (group, ";", NULL);
    len = strlen (cp);

    if (linepos > nameoutput) {
	if (len + linepos + 2 > outputlinelen) {
	    fprintf (out, ",\n%*s", nameoutput, "");
	    linepos = nameoutput;
	}
	else {
	    fputs (", ", out);
	    linepos += 2;
	}
    }

    fputs (cp, out);
    linepos += len;
}


static int
insert (struct mailname *np)
{
    struct mailname *mp;

    if (np->m_mbox == NULL)
	return 0;

    for (mp = np->m_type == LOCALHOST ? &localaddrs
	    : np->m_type == UUCPHOST ? &uuaddrs
	    : &netaddrs;
	    mp->m_next;
	    mp = mp->m_next)
	if (!strcasecmp (np->m_host ? np->m_host : "",
			 mp->m_next->m_host ? mp->m_next->m_host : "") &&
	    !strcasecmp (np->m_mbox ? np->m_mbox : "",
			 mp->m_next->m_mbox ? mp->m_next->m_mbox : "") &&
	    np->m_bcc == mp->m_next->m_bcc)
	    return 0;

    mp->m_next = np;
    recipients++;
    return 1;
}


static void
pl (void)
{
    int i;
    struct mailname *mp;

    printf ("-------\n\t-- Addresses --\nlocal:\t");
    for (mp = localaddrs.m_next; mp; mp = mp->m_next)
	printf ("%s%s%s", mp->m_mbox,
		mp->m_bcc ? "[BCC]" : "",
		mp->m_next ? ",\n\t" : "");

    printf ("\nnet:\t");
    for (mp = netaddrs.m_next; mp; mp = mp->m_next)
	printf ("%s%s@%s%s%s", mp->m_path ? mp->m_path : "",
		mp->m_mbox, mp->m_host,
		mp->m_bcc ? "[BCC]" : "",
		mp->m_next ? ",\n\t" : "");

    printf ("\nuucp:\t");
    for (mp = uuaddrs.m_next; mp; mp = mp->m_next)
	printf ("%s!%s%s%s", mp->m_host, mp->m_mbox,
		mp->m_bcc ? "[BCC]" : "",
		mp->m_next ? ",\n\t" : "");

    printf ("\n\t-- Folder Copies --\nfcc:\t");
    for (i = 0; i < fccind; i++)
	printf ("%s%s", fccfold[i], i + 1 < fccind ? ",\n\t" : "");
    printf ("\n");
}


static void
anno (void)
{
    struct mailname *mp;

    for (mp = localaddrs.m_next; mp; mp = mp->m_next)
	if (annoaux (mp) == NOTOK)
	    goto oops;

    for (mp = netaddrs.m_next; mp; mp = mp->m_next)
	if (annoaux (mp) == NOTOK)
	    goto oops;

    for (mp = uuaddrs.m_next; mp; mp = mp->m_next)
	if (annoaux (mp) == NOTOK)
	    break;

oops: ;
    close (pfd);
    pfd = NOTOK;
}


static int
annoaux (struct mailname *mp)
{
    int i;
    char buffer[BUFSIZ];

    snprintf (buffer, sizeof(buffer), "%s\n", adrformat (mp));
    i = strlen (buffer);

    return (write (pfd, buffer, i) == i ? OK : NOTOK);
}


static void
insert_fcc (struct headers *hdr, char *pp)
{
    char *cp;

    for (cp = pp; isspace ((unsigned char) *cp); cp++)
	continue;
    for (pp += strlen (pp) - 1; pp > cp && isspace ((unsigned char) *pp); pp--)
	continue;
    if (pp >= cp)
	*++pp = 0;
    if (*cp == 0)
	return;

    if (fccind >= FCCS)
	adios (NULL, "too many %ss", hdr->value);
    fccfold[fccind++] = mh_xstrdup(cp);
}

/*
 * BCC GENERATION
 */

static void
make_bcc_file (int dashstuff)
{
    int fd, i;
    pid_t child_id;
    char **vec;
    FILE *out;
    char *tfile = NULL, *program;

    if ((tfile = m_mktemp2(NULL, "bccs", NULL, &out)) == NULL) {
	adios(NULL, "unable to create temporary file in %s", get_temp_dir());
    }
    strncpy (bccfil, tfile, sizeof(bccfil));

    fprintf (out, "From: %s\n", fullfrom);
    fprintf (out, "Date: %s\n", dtime (&tclock, 0));
    if (msgid)
	fprintf (out, "Message-ID: %s\n", message_id (tclock, 0));
    if (subject)
	fprintf (out, "Subject: %s", subject);
    fprintf (out, "BCC:\n");

    /*
     * Use MIME encapsulation for Bcc messages
     */
    if (mime) {
	char *cp;

	/*
	 * Check if any lines in the message clash with the
	 * prefix for the MIME multipart separator.  If there
	 * is a clash, increment one of the letters in the
	 * prefix and check again.
	 */
	if ((cp = strchr(prefix, 'a')) == NULL)
	    adios (NULL, "lost prefix start");
	while (find_prefix () == NOTOK) {
	    if (*cp < 'z')
		(*cp)++;
	    else
		if (*++cp == 0)
		    adios (NULL, "can't find a unique delimiter string");
		else
		    (*cp)++;
	}

	fprintf (out, "%s: %s\n%s: multipart/digest; boundary=\"",
		 VRSN_FIELD, VRSN_VALUE, TYPE_FIELD);
	fprintf (out, "%s\"\n\n--%s\n\n", prefix, prefix);
    } else {
	fprintf (out, "\n------- Blind-Carbon-Copy\n\n");
    }

    fflush (out);

    /*
     * Do mhl filtering of Bcc messages instead
     * of MIME encapsulation.
     */
    if (filter != NULL) {
	for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
	    sleep (5);
	switch (child_id) {
	    case NOTOK: 
		adios ("fork", "unable to");

	    case OK: 
		dup2 (fileno (out), 1);

		vec = argsplit(mhlproc, &program, &i);
		vec[i++] = "-forward";
		vec[i++] = "-form";
		vec[i++] = filter;
		vec[i++] = tmpfil;

		/* was the flag -[no]dashstuffing specified? */
		if (dashstuff > 0)
		    vec[i++] = "-dashstuffing";
		else if (dashstuff < 0)
		    vec[i++] = "-nodashstuffing";
		vec[i] = NULL;

		execvp (program, vec);
		fprintf (stderr, "unable to exec ");
		perror (mhlproc);
		_exit (-1);

	    default: 
		pidXwait (child_id, mhlproc);
		break;
	}
    } else {
	if ((fd = open (tmpfil, O_RDONLY)) == NOTOK)
	    adios (tmpfil, "unable to re-open");

	/*
	 * If using MIME encapsulation, or if the -nodashstuffing
	 * flag was given, then just copy message.  Else do
	 * RFC934 quoting (dashstuffing).
	 */
	if (mime || dashstuff < 0)
	    cpydata (fd, fileno (out), tmpfil, bccfil);
	else
	    cpydgst (fd, fileno (out), tmpfil, bccfil);
	close (fd);
    }

    fseek (out, 0L, SEEK_END);
    if (mime)
	fprintf (out, "\n--%s--\n", prefix);
    else
	fprintf (out, "\n------- End of Blind-Carbon-Copy\n");
    fclose (out);
}


/*
 * Scan message to check if any lines clash with
 * the prefix of the MIME multipart separator.
 */

static int
find_prefix (void)
{
    int	result = OK;
    char buffer[BUFSIZ];
    FILE *in;

    if ((in = fopen (tmpfil, "r")) == NULL)
	adios (tmpfil, "unable to re-open");

    while (fgets (buffer, sizeof(buffer) - 1, in))
	if (buffer[0] == '-' && buffer[1] == '-') {
	    char *cp;

	    for (cp = buffer + strlen (buffer) - 1; cp >= buffer; cp--)
		if (!isspace ((unsigned char) *cp))
		    break;
	    *++cp = '\0';
	    if (strcmp (buffer + 2, prefix) == 0) {
		result = NOTOK;
		break;
	    }
	}

    fclose (in);
    return result;
}


#define	plural(x) (x == 1 ? "" : "s")

static void
chkadr (void)
{
    if (badadr && unkadr)
	die (NULL, "%d address%s unparsable, %d addressee%s undeliverable",
		badadr, plural (badadr), unkadr, plural (badadr));
    if (badadr)
	die (NULL, "%d address%s unparsable", badadr, plural (badadr));
    if (unkadr)
	die (NULL, "%d addressee%s undeliverable", unkadr, plural (unkadr));
}


static void
do_addresses (int bccque, int talk)
{
    int retval;
    int	state;
    struct mailname *lp;

    state = 0;
    for (lp = localaddrs.m_next; lp; lp = lp->m_next)
	if (lp->m_bcc ? bccque : !bccque) {
	    if (talk && !state)
		printf ("  -- Local Recipients --\n");
	    do_an_address (lp, talk);
	    state++;
	}

    state = 0;
    for (lp = uuaddrs.m_next; lp; lp = lp->m_next)
	if (lp->m_bcc ? bccque : !bccque) {
	    if (talk && !state)
		printf ("  -- UUCP Recipients --\n");
	    do_an_address (lp, talk);
	    state++;
	}

    state = 0;
    for (lp = netaddrs.m_next; lp; lp = lp->m_next)
	if (lp->m_bcc ? bccque : !bccque) {
	    if (talk && !state)
		printf ("  -- Network Recipients --\n");
	    do_an_address (lp, talk);
	    state++;
	}

    chkadr ();

    if (rp_isbad (retval = sm_waend ()))
	die (NULL, "problem ending addresses; %s", rp_string (retval));
}


/*
 * MTS-SPECIFIC INTERACTION
 */


/*
 * SENDMAIL/SMTP routines
 */

static void
post (char *file, int bccque, int talk, int eai, char *envelope,
      int oauth_flag, char *auth_svc)
{
    int	retval, i;
    pid_t child_id;

    if (verbose) {
	if (msgflags & MINV)
	    printf (" -- Posting for %s Recipients --\n",
		    bccque ? "Blind" : "Sighted");
	else
	    printf (" -- Posting for All Recipients --\n");
    }

    sigon ();

    if (sm_mts == MTS_SENDMAIL_PIPE) {
	char **argp, *program;
	int argc;

	for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
	    sleep (5);
	switch (child_id) {
	    case NOTOK: 
		adios ("fork", "unable to");

	    case OK:
		if (freopen( file, "r", stdin) == NULL) {
		    adios (file, "can't reopen for sendmail");
		}

		argp = argsplit(sendmail, &program, &argc);
		argp[argc++] = "-t"; /* read msg for recipients */
		argp[argc++] = "-i"; /* don't stop on "." */
		if (whomsw)
		    argp[argc++] = "-bv";
		if (snoop)
		    argp[argc++] = "-v";
		argp[argc] = NULL;

		execv (program, argp);
		adios (sendmail, "can't exec");

	    default: 
		pidXwait (child_id, NULL);
		break;
	}
    } else {
        const int fd = open (file, O_RDONLY);
        int eightbit = 0;

        if (fd == NOTOK) {
          die (file, "unable to re-open");
        }

        if (msgflags & MMIM  &&  cte != UNKNOWN) {
            /* MIME message with C-T-E header.  (BINARYMIME isn't
               supported, use 8BITMIME instead for binary.) */
            eightbit = cte != SEVENBIT;
        } else {
            if (scan_input (fd, &eightbit) == NOTOK) {
                close (fd);
                die (file, "problem reading from");
            }
        }

	if (rp_isbad (retval = sm_init (clientsw, serversw, port, watch,
					verbose, snoop, sasl, saslmech, user,
					oauth_flag ? auth_svc : NULL, tls))
		|| rp_isbad (retval = sm_winit (envelope, eai, eightbit))) {
	    close (fd);
	    die (NULL, "problem initializing server; %s", rp_string (retval));
	}

        do_addresses (bccque, talk && verbose);
        do_text (file, fd);
        close (fd);
        fflush (stdout);

        sm_end (!(msgflags & MINV) || bccque ? OK : DONE);
        sigoff ();

        if (verbose) {
            if (msgflags & MINV)
	        printf (" -- %s Recipient Copies Posted --\n",
		        bccque ? "Blind" : "Sighted");
            else
	        printf (" -- Recipient Copies Posted --\n");
        }

        fflush (stdout);
    }
}


/* Address Verification */

static void
verify_all_addresses (int talk, int eai, char *envelope, int oauth_flag,
                      char *auth_svc)
{
    int retval;
    struct mailname *lp;

    sigon ();

    if (!whomsw || checksw) {
        /* Not sending message body, so don't need to use 8BITMIME. */
        const int eightbit = 0;

	if (rp_isbad (retval = sm_init (clientsw, serversw, port, watch,
					verbose, snoop, sasl, saslmech, user,
					oauth_flag ? auth_svc : NULL, tls))
		|| rp_isbad (retval = sm_winit (envelope, eai, eightbit))) {
	    die (NULL, "problem initializing server; %s", rp_string (retval));
	}
    }

    if (talk && !whomsw)
	printf (" -- Address Verification --\n");
    if (talk && localaddrs.m_next)
	printf ("  -- Local Recipients --\n");
    for (lp = localaddrs.m_next; lp; lp = lp->m_next)
	do_an_address (lp, talk);

    if (talk && uuaddrs.m_next)
	printf ("  -- UUCP Recipients --\n");
    for (lp = uuaddrs.m_next; lp; lp = lp->m_next)
	do_an_address (lp, talk);

    if (talk && netaddrs.m_next)
	printf ("  -- Network Recipients --\n");
    for (lp = netaddrs.m_next; lp; lp = lp->m_next)
	do_an_address (lp, talk);

    chkadr ();
    if (talk && !whomsw)
	printf (" -- Address Verification Successful --\n");

    if (!whomsw || checksw)
	sm_end (DONE);

    fflush (stdout);
    sigoff ();
}


static void
do_an_address (struct mailname *lp, int talk)
{
    int retval;
    char *mbox, *host;
    char addr[BUFSIZ];

    switch (lp->m_type) {
	case LOCALHOST: 
	    mbox = lp->m_mbox;
	    host = lp->m_host;
	    strncpy (addr, mbox, sizeof(addr));
	    break;

	case UUCPHOST: 
	    mbox = auxformat (lp, 0);
	    host = NULL;
	    snprintf (addr, sizeof(addr), "%s!%s", lp->m_host, lp->m_mbox);
	    break;

	default: 		/* let SendMail decide if the host is bad  */
	    mbox = lp->m_mbox;
	    host = lp->m_host;
	    snprintf (addr, sizeof(addr), "%s at %s", mbox, host);
	    break;
    }

    if (talk)
	printf ("  %s%s", addr, whomsw && lp->m_bcc ? "[BCC]" : "");

    if (whomsw && !checksw) {
	putchar ('\n');
	return;
    }
    if (talk)
	printf (": ");
    fflush (stdout);

    switch (retval = sm_wadr (mbox, host,
			 lp->m_type != UUCPHOST ? lp->m_path : NULL)) {
	case RP_OK: 
	    if (talk)
		printf ("address ok\n");
	    break;

	case RP_NO: 
	case RP_USER: 
	    if (!talk)
		fprintf (stderr, "  %s: ", addr);
	    fprintf (talk ? stdout : stderr, "loses; %s\n",
			rp_string (retval));
	    unkadr++;
	    break;

	default: 
	    if (!talk)
		fprintf (stderr, "  %s: ", addr);
	    die (NULL, "unexpected response; %s", rp_string (retval));
    }

    fflush (stdout);
}


static void
do_text (char *file, int fd)
{
    int retval, state;
    char buf[BUFSIZ];

    lseek (fd, (off_t) 0, SEEK_SET);

    while ((state = read (fd, buf, sizeof(buf))) > 0) {
	if (rp_isbad (retval = sm_wtxt (buf, state)))
	    die (NULL, "problem writing text; %s\n", rp_string (retval));
    }

    if (state == NOTOK)
	die (file, "problem reading from");

    switch (retval = sm_wtend ()) {
	case RP_OK: 
	    break;

	case RP_NO: 
	case RP_NDEL: 
	    die (NULL, "posting failed; %s", rp_string (retval));

	default: 
	    die (NULL, "unexpected response; %s", rp_string (retval));
    }
}


/*
 * SIGNAL HANDLING
 */

static void
sigser (int i)
{
    NMH_UNUSED (i);

    (void) m_unlink (tmpfil);
    if (msgflags & MINV)
	(void) m_unlink (bccfil);

    if (!whomsw || checksw)
	sm_end (NOTOK);

    done (1);
}


static void
sigon (void)
{
    if (debug)
	return;

    hstat = SIGNAL2 (SIGHUP, sigser);
    istat = SIGNAL2 (SIGINT, sigser);
    qstat = SIGNAL2 (SIGQUIT, sigser);
    tstat = SIGNAL2 (SIGTERM, sigser);
}


static void
sigoff (void)
{
    if (debug)
	return;

    SIGNAL (SIGHUP, hstat);
    SIGNAL (SIGINT, istat);
    SIGNAL (SIGQUIT, qstat);
    SIGNAL (SIGTERM, tstat);
}

/*
 * FCC INTERACTION
 */

static void
p_refile (char *file)
{
    int i;

    if (fccind == 0)
	return;

    if (verbose)
	printf (" -- Filing Folder Copies --\n");
    for (i = 0; i < fccind; i++)
	fcc (file, fccfold[i]);
    if (verbose)
	printf (" -- Folder Copies Filed --\n");
}


/*
 * Call the `fileproc' to add the file to the folder.
 */

static void
fcc (char *file, char *folder)
{
    pid_t child_id;
    int i, status, argp;
    char fold[BUFSIZ];
    char **arglist, *program;

    if (verbose)
	printf ("  %sFcc %s: ", msgstate == RESENT ? "Resent-" : "", folder);
    fflush (stdout);

    for (i = 0; (child_id = fork ()) == NOTOK && i < 5; i++)
	sleep (5);

    switch (child_id) {
	case NOTOK: 
	    if (!verbose)
		fprintf (stderr, "  %sFcc %s: ",
			msgstate == RESENT ? "Resent-" : "", folder);
	    fprintf (verbose ? stdout : stderr, "no forks, so not ok\n");
	    break;

	case OK: 
	    /* see if we need to add `+' */
	    snprintf (fold, sizeof(fold), "%s%s",
		    *folder == '+' || *folder == '@' ? "" : "+", folder);

	    /* now exec the fileproc */

	    arglist = argsplit(fileproc, &program, &argp);
	    arglist[argp++] = "-link";
	    arglist[argp++] = "-file";
	    arglist[argp++] = file;
	    arglist[argp++] = fold;
	    arglist[argp] = NULL;
	    execvp (program, arglist);
	    _exit (-1);

	default: 
	    if ((status = pidwait (child_id, OK))) {
		if (!verbose)
		    fprintf (stderr, "  %sFcc %s: ",
			    msgstate == RESENT ? "Resent-" : "", folder);
		pidstatus (status, verbose ? stdout : stderr, NULL);
	    } else {
		if (verbose)
		    printf ("folder ok\n");
	    }
    }

    fflush (stdout);
}

/*
 * TERMINATION
 */

static void
die (char *what, char *fmt, ...)
{
    va_list ap;

    (void) m_unlink (tmpfil);
    if (msgflags & MINV)
	(void) m_unlink (bccfil);

    if (!whomsw || checksw)
	sm_end (NOTOK);

    va_start(ap, fmt);
    advertise (what, NULL, fmt, ap);
    va_end(ap);
    done (1);
}
