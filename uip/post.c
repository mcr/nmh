
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

#include <errno.h>
#include <signal.h>

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

#define FCCS		10	/* max number of fccs allowed */

#define	uptolow(c)	((isalpha(c) && isupper (c)) ? tolower (c) : c)

/* In the following array of structures, the numeric second field of the
   structures (minchars) is apparently used like this:

   -# : Switch can be abbreviated to # characters; switch hidden in -help.
   0  : Switch can't be abbreviated;               switch shown in -help.
   #  : Switch can be abbreviated to # characters; switch shown in -help. */

static struct swit switches[] = {
#define	ALIASW                    0
    { "alias aliasfile", 0 },
#define	CHKSW                     1
    { "check", -5 },			/* interface from whom */
#define	NCHKSW                    2
    { "nocheck", -7 },			/* interface from whom */
#define	DEBUGSW                   3
    { "debug", -5 },
#define	DISTSW                    4
    { "dist", -4 },			/* interface from dist */
#define	FILTSW                    5
    { "filter filterfile", 0 },
#define	NFILTSW                   6
    { "nofilter", 0 },
#define	FRMTSW                    7
    { "format", 0 },
#define	NFRMTSW                   8
    { "noformat", 0 },
#define	LIBSW                     9
    { "library directory", -7 },	/* interface from send, whom */
#define	MIMESW                   10
    { "mime", 0 },
#define	NMIMESW                  11
    { "nomime", 0 },
#define	MSGDSW                   12
    { "msgid", 0 },
#define	NMSGDSW                  13
    { "nomsgid", 0 },
#define	VERBSW                   14
    { "verbose", 0 },
#define	NVERBSW                  15
    { "noverbose", 0 },
#define	WATCSW                   16
    { "watch", 0 },
#define	NWATCSW                  17
    { "nowatch", 0 },
#define	WHOMSW                   18
    { "whom", -4 },			/* interface from whom */
#define	WIDTHSW                  19
    { "width columns", 0 },
#define VERSIONSW                20
    { "version", 0 },
#define	HELPSW                   21
    { "help", 0 },
#define BITSTUFFSW               22
    { "dashstuffing", -12 },		/* should we dashstuff BCC messages? */
#define NBITSTUFFSW              23
    { "nodashstuffing", -14 },
#define	ANNOSW                   24
    { "idanno number", -6 },		/* interface from send    */
#define	CLIESW                   25
    { "client host", -6 },
#define	SERVSW                   26
    { "server host", 6 },		/* specify alternate SMTP server */
#define	SNOOPSW                  27
    { "snoop", -5 },			/* snoop the SMTP transaction */
#define	PARTSW                   28
    { "partno", -6 },
#define	QUEUESW                  29
    { "queued", -6 },
#define SASLSW                   30
    { "sasl", SASLminc(-4) },
#define NOSASLSW                 31
    { "nosasl", SASLminc(-6) },
#define SASLMXSSFSW              32
    { "saslmaxssf", SASLminc(-10) },
#define SASLMECHSW               33
    { "saslmech", SASLminc(-5) },
#define USERSW                   34
    { "user", SASLminc(-4) },
#define PORTSW			 35
    { "port server port name/number", 4 },
#define TLSSW			 36
    { "tls", TLSminc(-3) },
#define NTLSSW                   37
    { "notls", TLSminc(-5) },
#define FILEPROCSW		 38
    { "fileproc", -4 },
#define MHLPROCSW		 39
    { "mhlproc", -3 },
    { NULL, 0 }
};


struct headers {
    char *value;
    unsigned int flags;
    unsigned int set;
};

/*
 * flags for headers->flags
 */
#define	HNOP  0x0000		/* just used to keep .set around          */
#define	HBAD  0x0001		/* bad header - don't let it through      */
#define	HADR  0x0002		/* header has an address field            */
#define	HSUB  0x0004		/* Subject: header                        */
#define	HTRY  0x0008		/* try to send to addrs on header         */
#define	HBCC  0x0010		/* don't output this header               */
#define	HMNG  0x0020		/* munge this header                      */
#define	HNGR  0x0040		/* no groups allowed in this header       */
#define	HFCC  0x0080		/* FCC: type header                       */
#define	HNIL  0x0100		/* okay for this header not to have addrs */
#define	HIGN  0x0200		/* ignore this header                     */
#define	HDCC  0x0400		/* another undocumented feature           */
#define HONE  0x0800		/* Only one address allowed		  */
#define HEFM  0x1000		/* Envelope-From: header		  */

/*
 * flags for headers->set
 */
#define	MFRM  0x0001		/* we've seen a From:        */
#define	MDAT  0x0002		/* we've seen a Date:        */
#define	MRFM  0x0004		/* we've seen a Resent-From: */
#define	MVIS  0x0008		/* we've seen sighted addrs  */
#define	MINV  0x0010		/* we've seen blind addrs    */
#define MSND  0x0020		/* we've seen a Sender:      */
#define MRSN  0x0040		/* We've seen a Resent-Sendr:*/
#define MEFM  0x0080		/* We've seen Envelope-From: */


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
    { NULL,                0,                   0 }
};

static short fccind = 0;	/* index into fccfold[] */
static short outputlinelen = OUTPUTLINELEN;

static int pfd = NOTOK;		/* fd to write annotation list to        */
static uid_t myuid= -1;		/* my user id                            */
static gid_t mygid= -1;		/* my group id                           */
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
static int saslssf=-1;		/* Our maximum SSF for SASL              */
static char *saslmech=NULL;	/* Force use of particular SASL mech     */
static char *user=NULL;		/* Authenticate as this user             */
static char *port="smtp";	/* Name of server port for SMTP		 */
static int tls=0;		/* Use TLS for encryption		 */
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
static char signature[BUFSIZ];	/* my signature                  */
static char *filter = NULL;	/* the filter for BCC'ing        */
static char *subject = NULL;	/* the subject field for BCC'ing */
static char *fccfold[FCCS];	/* foldernames for FCC'ing       */

static struct headers  *hdrtab;	/* table for the message we're doing */

static struct mailname localaddrs;		/* local addrs     */
static struct mailname netaddrs;		/* network addrs   */
static struct mailname uuaddrs;			/* uucp addrs      */
static struct mailname tmpaddrs;		/* temporary queue */

static int snoop      = 0;
static char *clientsw = NULL;
static char *serversw = NULL;

extern struct smtp sm_reply;

static char prefix[] = "----- =_aaaaaaaaaa";

static char *partno = NULL;
static int queued = 0;

/*
 * static prototypes
 */
static void putfmt (char *, char *, FILE *);
static void start_headers (void);
static void finish_headers (FILE *);
static int get_header (char *, struct headers *);
static int putadr (char *, char *, struct mailname *, FILE *, unsigned int);
static void putgrp (char *, char *, FILE *, unsigned int);
static int insert (struct mailname *);
static void pl (void);
static void anno (void);
static int annoaux (struct mailname *);
static void insert_fcc (struct headers *, unsigned char *);
static void make_bcc_file (int);
static void verify_all_addresses (int, char *);
static void chkadr (void);
static void sigon (void);
static void sigoff (void);
static void p_refile (char *);
static void fcc (char *, char *);
static void die (char *, char *, ...);
static void post (char *, int, int, char *);
static void do_text (char *file, int fd);
static void do_an_address (struct mailname *, int);
static void do_addresses (int, int);
static int find_prefix (void);


int
main (int argc, char **argv)
{
    int state, compnum, dashstuff = 0;
    char *cp, *msg = NULL, **argp, **arguments, *envelope;
    char buf[BUFSIZ], name[NAMESZ];
    FILE *in, *out;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* foil search of user profile/context */
    if (context_foil (NULL) == -1)
	done (1);

    mts_init (invo_name);
    arguments = getarguments (invo_name, argc, argv, 0);
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
		    snprintf (buf, sizeof(buf), "%s [switches] file", invo_name);
		    print_help (buf, switches, 0);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

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

		case QUEUESW:
		    queued++;
		    continue;
		
		case SASLSW:
		    sasl++;
		    continue;

		case NOSASLSW:
		    sasl = 0;
		    continue;

		case SASLMXSSFSW:
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    saslssf = atoi(cp);
		    continue;
		
		case SASLMECHSW:
		    if (!(saslmech = *argp++) || *saslmech == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		
		case USERSW:
		    if (!(user = *argp++) || *user == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case PORTSW:
		    if (!(port = *argp++) || *port == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case TLSSW:
		    tls++;
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
	discard (out = stdout);	/* XXX: reference discard() to help loader */
    } else {
	if (whomsw) {
	    if ((out = fopen ("/dev/null", "w")) == NULL)
		adios ("/dev/null", "unable to open");
	} else {
            char *cp = m_mktemp(m_maildir(invo_name), NULL, &out);
            if (cp == NULL) {
                cp = m_mktemp2(NULL, invo_name, NULL, &out);
                if (cp == NULL) {
		    adios ("post", "unable to create temporary file");
                }
            }
            strncpy(tmpfil, cp, sizeof(tmpfil));
	    chmod (tmpfil, 0600);
	}
    }

    hdrtab = msgstate == NORMAL ? NHeaders : RHeaders;

    for (compnum = 1, state = FLD;;) {
	switch (state = m_getfld (state, name, buf, sizeof(buf), in)) {
	    case FLD: 
	    case FLDEOF: 
	    case FLDPLUS: 
		compnum++;
		cp = add (buf, NULL);
		while (state == FLDPLUS) {
		    state = m_getfld (state, name, buf, sizeof(buf), in);
		    cp = add (buf, cp);
		}
		putfmt (name, cp, out);
		free (cp);
		if (state != FLDEOF)
		    continue;
		finish_headers (out);
		break;

	    case BODY: 
	    case BODYEOF: 
		finish_headers (out);
		if (whomsw)
		    break;
		fprintf (out, "\n%s", buf);
		while (state == BODY) {
		    state = m_getfld (state, name, buf, sizeof(buf), in);
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
     * - If we were given multiple addresses in the From: header, use
     *   the Sender: address
     * - Otherwise, use the address on the From: line
     */

    if (msgflags & MEFM) {
    	envelope = efrom;
    } else if (fromcount > 1) {
    	envelope = sender;
    } else {
    	envelope = from;
    }

    /* If we are doing a "whom" check */
    if (whomsw) {
	verify_all_addresses (1, envelope);
	done (0);
    }

    if (msgflags & MINV) {
	make_bcc_file (dashstuff);
	if (msgflags & MVIS) {
	    verify_all_addresses (verbose, envelope);
	    post (tmpfil, 0, verbose, envelope);
	}
	post (bccfil, 1, verbose, envelope);
	unlink (bccfil);
    } else {
	post (tmpfil, 0, isatty (1), envelope);
    }

    p_refile (tmpfil);
    unlink (tmpfil);

    if (verbose)
	printf (partno ? "Partial Message #%s Processed\n" : "Message Processed\n",
		partno);
    done (0);
    return 1;
}


/*
 * DRAFT GENERATION
 */

static void
putfmt (char *name, char *str, FILE *out)
{
    int count, grp, i, keep;
    char *cp, *pp, *qp;
    char namep[BUFSIZ];
    struct mailname *mp = NULL, *np = NULL;
    struct headers *hdr;

    while (*str == ' ' || *str == '\t')
	str++;

    if (msgstate == NORMAL && uprf (name, "resent")) {
	advise (NULL, "illegal header line -- %s:", name);
	badmsg++;
	return;
    }

    if ((i = get_header (name, hdrtab)) == NOTOK) {
	fprintf (out, "%s: %s", name, str);
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
	subject = subject ? add (str, add ("\t", subject)) : getcpy (str);
    if (hdr->flags & HFCC) {
	if ((cp = strrchr(str, '\n')))
	    *cp = 0;
	for (cp = pp = str; (cp = strchr(pp, ',')); pp = cp) {
	    *cp++ = 0;
	    insert_fcc (hdr, pp);
	}
	insert_fcc (hdr, pp);
	return;
    }

    if (!(hdr->flags & HADR)) {
	fprintf (out, "%s: %s", name, str);
	return;
    }

    tmpaddrs.m_next = NULL;
    for (count = 0; (cp = getname (str)); count++)
	if ((mp = getm (cp, NULL, 0, AD_HOST, NULL))) {
	    if (tmpaddrs.m_next)
		np->m_next = mp;
	    else
		tmpaddrs.m_next = mp;
	    np = mp;
	}
	else
	    if (hdr->flags & HTRY)
		badadr++;
	    else
		badmsg++;

    if (count < 1) {
	if (hdr->flags & HNIL)
	    fprintf (out, "%s: %s", name, str);
	else {
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
		if (!(mp = getm (cp, NULL, 0, AD_HOST, NULL))) {
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
		if (mp->m_ingrp)
		    grp++;
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
    unsigned char  *cp;
    char myhost[BUFSIZ], sigbuf[BUFSIZ];
    struct mailname *mp;

    myuid = getuid ();
    mygid = getgid ();
    time (&tclock);

    strncpy (from, getlocaladdr(), sizeof(from));
    strncpy (myhost, LocalName (0), sizeof(myhost));

    for (cp = myhost; *cp; cp++)
	*cp = uptolow (*cp);

    if ((cp = getfullname ()) && *cp) {
	strncpy (sigbuf, cp, sizeof(sigbuf));
	snprintf (signature, sizeof(signature), "%s <%s>",
		sigbuf, getlocaladdr());
	if ((cp = getname (signature)) == NULL)
	    adios (NULL, "getname () failed -- you lose extraordinarily big");
	if ((mp = getm (cp, NULL, 0, AD_HOST, NULL)) == NULL)
	    adios (NULL, "bad signature '%s'", sigbuf);
	mnfree (mp);
	while (getname (""))
	    continue;
    } else {
	strncpy (signature, getlocaladdr(), sizeof(signature));
    }
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
	    }

	    if (fromcount > 1 && seensender == 0) {
		advise (NULL, "A Sender: header is required with multiple "
			"From: addresses");
		badmsg++;
	    }

	    if (whomsw)
		break;

	    fprintf (out, "Date: %s\n", dtime (&tclock, 0));
	    if (msgid)
		fprintf (out, "Message-ID: <%d.%ld@%s>\n",
			(int) getpid (), (long) tclock, LocalName (1));
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
	    }
	    if (fromcount > 1 && seensender == 0) {
		advise (NULL, "A Resent-Sender: header is required with "
			"multiple Resent-From: addresses");
		badmsg++;
	    }

	    if (whomsw)
		break;

	    fprintf (out, "Resent-Date: %s\n", dtime (&tclock, 0));
	    if (msgid)
		fprintf (out, "Resent-Message-ID: <%d.%ld@%s>\n",
			(int) getpid (), (long) tclock, LocalName (1));
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
	if (!mh_strcasecmp (header, h->value))
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
    if ((flags & (HBCC | HDCC | HEFM)) || mp->m_ingrp)
	return 1;

    if (!nameoutput) {
	fprintf (out, "%s: ", name);
	linepos += (nameoutput = strlen (name) + 2);
    }

    if (*aka && mp->m_type != UUCPHOST && !mp->m_pers)
	mp->m_pers = getcpy (aka);
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

    if (flags & HBCC)
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
	if (!mh_strcasecmp (np->m_host, mp->m_next->m_host)
		&& !mh_strcasecmp (np->m_mbox, mp->m_next->m_mbox)
		&& np->m_bcc == mp->m_next->m_bcc)
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
insert_fcc (struct headers *hdr, unsigned char *pp)
{
    unsigned char *cp;

    for (cp = pp; isspace (*cp); cp++)
	continue;
    for (pp += strlen (pp) - 1; pp > cp && isspace (*pp); pp--)
	continue;
    if (pp >= cp)
	*++pp = 0;
    if (*cp == 0)
	return;

    if (fccind >= FCCS)
	adios (NULL, "too many %ss", hdr->value);
    fccfold[fccind++] = getcpy (cp);
}

/*
 * BCC GENERATION
 */

static void
make_bcc_file (int dashstuff)
{
    int fd, i;
    pid_t child_id;
    char *vec[6];
    FILE *out;
    char *tfile = NULL;

    tfile = m_mktemp2(NULL, "bccs", NULL, &out);
    if (tfile == NULL) adios("bcc", "unable to create temporary file");
    chmod (bccfil, 0600);
    strncpy (bccfil, tfile, sizeof(bccfil));

    fprintf (out, "Date: %s\n", dtime (&tclock, 0));
    if (msgid)
	fprintf (out, "Message-ID: <%d.%ld@%s>\n",
		(int) getpid (), (long) tclock, LocalName (1));
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
	vec[0] = r1bindex (mhlproc, '/');

	for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
	    sleep (5);
	switch (child_id) {
	    case NOTOK: 
		adios ("fork", "unable to");

	    case OK: 
		dup2 (fileno (out), 1);

		i = 1;
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

		execvp (mhlproc, vec);
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
    unsigned char buffer[BUFSIZ];
    FILE *in;

    if ((in = fopen (tmpfil, "r")) == NULL)
	adios (tmpfil, "unable to re-open");

    while (fgets (buffer, sizeof(buffer) - 1, in))
	if (buffer[0] == '-' && buffer[1] == '-') {
	    unsigned char *cp;

	    for (cp = buffer + strlen (buffer) - 1; cp >= buffer; cp--)
		if (!isspace (*cp))
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
post (char *file, int bccque, int talk, char *envelope)
{
    int fd, onex;
    int	retval;

    onex = !(msgflags & MINV) || bccque;
    if (verbose) {
	if (msgflags & MINV)
	    printf (" -- Posting for %s Recipients --\n",
		    bccque ? "Blind" : "Sighted");
	else
	    printf (" -- Posting for All Recipients --\n");
    }

    sigon ();

    if (rp_isbad (retval = sm_init (clientsw, serversw, port, watch, verbose,
				    snoop, onex, queued, sasl, saslssf,
				    saslmech, user, tls))
	    || rp_isbad (retval = sm_winit (envelope)))
	die (NULL, "problem initializing server; %s", rp_string (retval));

    do_addresses (bccque, talk && verbose);
    if ((fd = open (file, O_RDONLY)) == NOTOK)
	die (file, "unable to re-open");
    do_text (file, fd);
    close (fd);
    fflush (stdout);

    sm_end (onex ? OK : DONE);
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


/* Address Verification */

static void
verify_all_addresses (int talk, char *envelope)
{
    int retval;
    struct mailname *lp;

    sigon ();

    if (!whomsw || checksw)
	if (rp_isbad (retval = sm_init (clientsw, serversw, port, watch,
					verbose, snoop, 0, queued, sasl,
					saslssf, saslmech, user, tls))
		|| rp_isbad (retval = sm_winit (envelope)))
	    die (NULL, "problem initializing server; %s", rp_string (retval));

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

    unlink (tmpfil);
    if (msgflags & MINV)
	unlink (bccfil);

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
    int i, status;
    char fold[BUFSIZ];

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
	    execlp (fileproc, r1bindex (fileproc, '/'),
		    "-link", "-file", file, fold, NULL);
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

    unlink (tmpfil);
    if (msgflags & MINV)
	unlink (bccfil);

    if (!whomsw || checksw)
	sm_end (NOTOK);

    va_start(ap, fmt);
    advertise (what, NULL, fmt, ap);
    va_end(ap);
    done (1);
}
