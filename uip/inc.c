/* inc.c -- incorporate messages from a maildrop into a folder
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#ifdef MAILGROUP
/*
 * Thu Feb 12 21:00 CST 2015            Marcin Cieslak <saper@saper.info>
 *    Replaced setgid() calls with setegid() so that it works with dot
 *    locking on FreeBSD.  setegid() should be supported on modern POSIX
 *    systems.
 *
 * Revised: Sat Apr 14 17:08:17 PDT 1990 (marvit@hplabs)
 *    Added hpux hacks to set and reset gid to be "mail" as needed. The reset
 *    is necessary so inc'ed mail is the group of the inc'er, rather than
 *    "mail". We setgid to egid only when [un]locking the mail file. This
 *    is also a major security precaution which will not be explained here.
 *
 * Fri Feb  7 16:04:57 PST 1992		John Romine <bug-mh@ics.uci.edu>
 *   NB: I'm not 100% sure that this setgid stuff is secure even now.
 *
 * See the *GROUPPRIVS() macros later. I'm reasonably happy with the setgid
 * attribute. Running setuid root is probably not a terribly good idea, though.
 *       -- Peter Maydell <pmaydell@chiark.greenend.org.uk>, 04/1998
 *
 * Peter Maydell's patch slightly modified for nmh 0.28-pre2.
 * Ruud de Rooij <ruud@debian.org>  Wed, 22 Jul 1998 13:24:22 +0200
 */
#endif

#include "h/mh.h"
#include "sbr/ambigsw.h"
#include "sbr/path.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/error.h"
#include "h/utils.h"
#include <fcntl.h>
#include "h/dropsbr.h"
#include "h/popsbr.h"
#include "h/fmt_scan.h"
#include "h/scansbr.h"
#include "h/signals.h"
#include "h/tws.h"
#include "h/mts.h"
#include "h/done.h"
#include "sbr/lock_file.h"
#include "sbr/m_maildir.h"
#include "sbr/m_mktemp.h"

#ifndef TLS_SUPPORT
# define TLSminc(a) (a)
#else
# define TLSminc(a)  0
#endif

#define INC_SWITCHES \
    X("audit audit-file", 0, AUDSW) \
    X("noaudit", 0, NAUDSW) \
    X("changecur", 0, CHGSW) \
    X("nochangecur", 0, NCHGSW) \
    X("file name", 0, FILESW) \
    X("form formatfile", 0, FORMSW) \
    X("format string", 5, FMTSW) \
    X("host hostname", 0, HOSTSW) \
    X("user username", 0, USERSW) \
    X("port name/number", 0, PORTSW) \
    X("silent", 0, SILSW) \
    X("nosilent", 0, NSILSW) \
    X("truncate", 0, TRNCSW) \
    X("notruncate", 0, NTRNCSW) \
    X("width columns", 0, WIDTHSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("snoop", 0, SNOOPSW) \
    X("sasl", 0, SASLSW) \
    X("nosasl", 0, NOSASLSW) \
    X("saslmech", 0, SASLMECHSW) \
    X("initialtls", TLSminc(-10), INITTLSSW) \
    X("notls", TLSminc(-5), NOTLSSW) \
    X("certverify", TLSminc(-10), CERTVERSW) \
    X("nocertverify", TLSminc(-12), NOCERTVERSW) \
    X("authservice", 0, AUTHSERVICESW) \
    X("proxy command", 0, PROXYSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(INC);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(INC, switches);
#undef X

/*
 * flags for the mail source
 */
#define INC_FILE  0
#define INC_POP   1

static struct Maildir_entry {
	char *filename;
	time_t mtime;
} *Maildir = NULL;
static int num_maildir_entries = 0;
static bool snoop;

typedef struct {
    FILE *mailout;
    long written;
} pop_closure;

extern char response[];

/* This is an attempt to simplify things by putting all the
 * privilege ops into macros.
 * *GROUPPRIVS() is related to handling the setgid MAIL property,
 * and only applies if MAILGROUP is defined.
 * Basically, SAVEGROUPPRIVS() is called right at the top of main()
 * to initialise things, and then DROPGROUPPRIVS() and GETGROUPPRIVS()
 * do the obvious thing.
 *
 * There's probably a better implementation if we're allowed to use
 * BSD-style setreuid() rather than using POSIX saved-ids.
 * Anyway, if you're euid root it's a bit pointless to drop the group
 * permissions...
 *
 * I'm pretty happy that the security is good provided we aren't setuid root.
 * The only things we trust with group=mail privilege are lkfopen()
 * and lkfclose().
 */

/*
 * For setting and returning to "mail" gid
 */
#ifdef MAILGROUP
static gid_t return_gid;
#define TRYDROPGROUPPRIVS() DROPGROUPPRIVS()
#define DROPGROUPPRIVS() \
    if (setegid(getgid()) != 0) { \
        adios ("setegid", "unable to restore group to %ld", (long) getgid()); \
    }
#define GETGROUPPRIVS() \
    if (setegid(return_gid) != 0) { \
        adios ("setegid", "unable to set group to %ld", (long) return_gid); \
    }
#define SAVEGROUPPRIVS() return_gid = getegid()
#else
/* define *GROUPPRIVS() as null; this avoids having lots of "#ifdef MAILGROUP"s */
#define TRYDROPGROUPPRIVS()
#define DROPGROUPPRIVS()
#define GETGROUPPRIVS()
#define SAVEGROUPPRIVS()
#endif /* not MAILGROUP */

/* these variables have to be globals so that done() can correctly clean up the lockfile */
static bool locked;
static char *newmail;
static FILE *in;

/*
 * prototypes
 */
static int maildir_srt(const void *va, const void *vb) PURE;
static void inc_done(int) NORETURN;
static int pop_action(void *closure, char *);

static int
maildir_srt(const void *va, const void *vb)
{
    const struct Maildir_entry *a = va, *b = vb;
    if (a->mtime > b->mtime)
      return 1;
    if (a->mtime < b->mtime)
      return -1;
    return 0;
}

int
main (int argc, char **argv)
{
    static int inc_type;
    bool chgflag;
    int trnflag = 1;
    bool noisy;
    int width = -1;
    int hghnum = 0, msgnum = 0;
    FILE *pf = NULL;
    bool sasl, tls, noverify;
    int incerr = 0; /* <0 if inc hits an error which means it should not truncate mailspool */
    char *cp, *maildir = NULL, *folder = NULL;
    char *format = NULL, *form = NULL;
    char *host = NULL, *port = NULL, *user = NULL, *proxy = NULL;
    char *audfile = NULL, *from = NULL, *saslmech = NULL, *auth_svc = NULL;
    char buf[BUFSIZ], **argp, *nfs, **arguments;
    struct msgs *mp = NULL;
    struct stat st, s1;
    FILE *aud = NULL;
    char b[PATH_MAX + 1];
    char *maildir_copy = NULL;	/* copy of mail directory because the static gets overwritten */

    int nmsgs, nbytes;
    char *MAILHOST_env_variable;
    set_done(inc_done);

/* absolutely the first thing we do is save our privileges,
 * and drop them if we can.
 */
    SAVEGROUPPRIVS();
    TRYDROPGROUPPRIVS();

    if (nmh_init(argv[0], true, true)) { return 1; }

    mts_init ();
    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /*
     * Scheme is:
     *        use MAILHOST environment variable if present,
     *  else try Hesiod.
     *  If that fails, use the default (if any)
     *  provided by mts.conf in mts_init()
     */
    if ((MAILHOST_env_variable = getenv("MAILHOST")) != NULL)
	pophost = MAILHOST_env_variable;
    /*
     * If there is a valid "pophost" entry in mts.conf,
     * then use it as the default host.
     */
    if (pophost && *pophost)
	host = pophost;

    sasl = tls = false;
    chgflag = noisy = noverify = true;
    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
	    case AMBIGSW:
		ambigsw (cp, switches);
		done (1);
	    case UNKWNSW:
		die("-%s unknown", cp);

	    case HELPSW:
		snprintf (buf, sizeof(buf), "%s [+folder] [switches]", invo_name);
		print_help (buf, switches, 1);
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case AUDSW:
		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument to %s", argp[-2]);
		audfile = mh_xstrdup(m_maildir(cp));
		continue;
	    case NAUDSW:
		audfile = NULL;
		continue;

	    case CHGSW:
                chgflag = true;
		continue;
	    case NCHGSW:
                chgflag = false;
		continue;

	    /*
	     * The flag `trnflag' has the value:
	     *
	     * 2 if -truncate is given
	     * 1 by default (truncating is default)
	     * 0 if -notruncate is given
	     */
	    case TRNCSW:
		trnflag = 2;
		continue;
	    case NTRNCSW:
		trnflag = 0;
		continue;

	    case FILESW:
		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument to %s", argp[-2]);
		from = path (cp, TFILE);

		/*
		 * If the truncate file is in default state,
		 * change to not truncate.
		 */
		if (trnflag == 1)
		    trnflag = 0;
		continue;

	    case SILSW:
                noisy = false;
		continue;
	    case NSILSW:
                noisy = true;
		continue;

	    case FORMSW:
		if (!(form = *argp++) || *form == '-')
		    die("missing argument to %s", argp[-2]);
		format = NULL;
		continue;
	    case FMTSW:
		if (!(format = *argp++) || *format == '-')
		    die("missing argument to %s", argp[-2]);
		form = NULL;
		continue;

	    case WIDTHSW:
		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument to %s", argp[-2]);
		width = atoi (cp);
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
		if (!(user = *argp++) || *user == '-')
		    die("missing argument to %s", argp[-2]);
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
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		die("only one folder at a time!");
            folder = pluspath (cp);
	} else {
	    die("usage: %s [+folder] [switches]", invo_name);
	}
    }

    /* NOTE: above this point you should use TRYDROPGROUPPRIVS(),
     * not DROPGROUPPRIVS().
     */
    if (host && !*host)
	host = NULL;

    /* guarantee dropping group privileges; we might not have done so earlier */
    DROPGROUPPRIVS();

    /* Source of mail;  -from overrides any -host. */
    inc_type = host && !from ? INC_POP : INC_FILE;

    if (inc_type == INC_POP) {
        /* Mail from a POP server. */
	int tlsflag = 0;

	if (auth_svc == NULL) {
	    if (saslmech  &&  ! strcasecmp(saslmech, "xoauth2")) {
		die("must specify -authservice with -saslmech xoauth2");
	    }
	} else {
	    if (user == NULL) {
		die("must specify -user with -saslmech xoauth2");
	    }
	}

	if (tls)
	    tlsflag |= P_INITTLS;

	if (noverify)
	    tlsflag |= P_NOVERIFY;

	/*
	 * initialize POP connection
	 */
	if (pop_init (host, port, user, proxy, snoop, sasl, saslmech,
		      tlsflag, auth_svc) == NOTOK)
	    die("%s", response);

	/* Check if there are any messages */
	if (pop_stat (&nmsgs, &nbytes) == NOTOK)
	    die("%s", response);

	if (nmsgs == 0) {
	    pop_quit();
	    die("no mail to incorporate");
	}

    } else if (inc_type == INC_FILE) {
        /* Mail from a spool file, or Maildir. */
	if (from)
	    newmail = from;
	else if ((newmail = getenv ("MAILDROP")) && *newmail)
	    newmail = m_mailpath (newmail);
	else if ((newmail = context_find ("maildrop")) && *newmail)
	    newmail = m_mailpath (newmail);
	else {
	    newmail = concat (MAILDIR, "/", MAILFIL, NULL);
	}
	if (stat (newmail, &s1) == NOTOK || s1.st_size == 0)
	    die("no mail to incorporate");
 	if (s1.st_mode & S_IFDIR) {
 	    DIR *md;
 	    struct dirent *de;
 	    struct stat ms;
 	    int i;
 	    i = 0;
 	    cp = concat (newmail, "/new", NULL);
 	    if ((md = opendir(cp)) == NULL)
 		die("unable to open %s", cp);
 	    while ((de = readdir (md)) != NULL) {
 		if (de->d_name[0] == '.')
 		    continue;
 		if (i >= num_maildir_entries) {
 		    if ((Maildir = realloc(Maildir, sizeof(*Maildir) * (2*i+16))) == NULL)
 			die("not enough memory for %d messages", 2*i+16);
 		    num_maildir_entries = 2*i+16;
 		}
 		Maildir[i].filename = concat (cp, "/", de->d_name, NULL);
 		if (stat(Maildir[i].filename, &ms) != 0)
 	           adios (Maildir[i].filename, "couldn't get delivery time");
 		Maildir[i].mtime = ms.st_mtime;
 		i++;
 	    }
 	    free (cp);
 	    closedir (md);
 	    cp = concat (newmail, "/cur", NULL);
 	    if ((md = opendir(cp)) == NULL)
 		die("unable to open %s", cp);
 	    while ((de = readdir (md)) != NULL) {
 		if (de->d_name[0] == '.')
 		    continue;
 		if (i >= num_maildir_entries) {
 		    if ((Maildir = realloc(Maildir, sizeof(*Maildir) * (2*i+16))) == NULL)
 			die("not enough memory for %d messages", 2*i+16);
 		    num_maildir_entries = 2*i+16;
 		}
 		Maildir[i].filename = concat (cp, "/", de->d_name, NULL);
 		if (stat(Maildir[i].filename, &ms) != 0)
 	           adios (Maildir[i].filename, "couldn't get delivery time");
 		Maildir[i].mtime = ms.st_mtime;
 		i++;
 	    }
 	    free (cp);
 	    closedir (md);
 	    if (i == 0)
 	        die("no mail to incorporate");
 	    num_maildir_entries = i;
 	    qsort (Maildir, num_maildir_entries, sizeof(*Maildir), maildir_srt);
 	}

	cp = mh_xstrdup(newmail);
	newmail = cp;
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (!folder)
	folder = getfolder (0);
    maildir = m_maildir (folder);
    maildir_copy = mh_xstrdup(maildir);

    if (!folder_exists(maildir)) {
        /* If the folder doesn't exist, and we're given the -silent flag,
         * just fail.
         */
        if (noisy)
            create_folder(maildir, 0, done);
        else
            done (1);
    }

    if (chdir (maildir) == NOTOK)
	adios (maildir, "unable to change directory to");

    /* read folder and create message structure */
    if (!(mp = folder_read (folder, 0)))
	die("unable to read folder %s", folder);

    if (inc_type == INC_FILE && Maildir == NULL) {
        /* Mail from a spool file. */

	if (access (newmail, W_OK) != NOTOK) {
	    locked = true;
	    if (trnflag) {
		SIGNAL (SIGHUP, SIG_IGN);
		SIGNAL (SIGINT, SIG_IGN);
		SIGNAL (SIGQUIT, SIG_IGN);
		SIGNAL (SIGTERM, SIG_IGN);
	    }

            GETGROUPPRIVS();       /* Reset gid to lock mail file */
            in = lkfopenspool (newmail, "r");
            DROPGROUPPRIVS();
            if (in == NULL)
		die("unable to lock and fopen %s", newmail);
	    fstat (fileno(in), &s1);
	} else {
	    trnflag = 0;
	    if ((in = fopen (newmail, "r")) == NULL)
		adios (newmail, "unable to read");
	}
    }

    /* This shouldn't be necessary but it can't hurt. */
    DROPGROUPPRIVS();

    if (audfile) {
	int i;
	if ((i = stat (audfile, &st)) == NOTOK)
	    inform("Creating Receive-Audit: %s", audfile);
	if ((aud = fopen (audfile, "a")) == NULL)
	    adios (audfile, "unable to append to");
	if (i == NOTOK)
	    chmod (audfile, m_gmprot ());

	if (from)
	    fprintf (aud, "<<inc>> %s -ms %s\n", dtimenow(0), from);
	else {
	    if (host)
	    	fprintf (aud, "<<inc>> %s -host %s -user %s\n", dtimenow(0),
			 host, user);
	    else
		fprintf (aud, "<<inc>> %s\n", dtimenow (0));
	}
    }

    /* Get new format string */
    nfs = new_fs (form, format, FORMAT);

    if (noisy) {
	printf ("Incorporating new mail into %s...\n\n", folder);
	fflush (stdout);
    }


    /*
     * Get the mail from a POP server
     */
    if (inc_type == INC_POP) {
        /* Mail from a POP server. */
	int i;
        pop_closure pc;

        hghnum = msgnum = mp->hghmsg;
	for (i = 1; i <= nmsgs; i++) {
	    charstring_t scanl = NULL;

	    msgnum++;
            cp = mh_xstrdup(m_name (msgnum));
            if ((pf = fopen (cp, "w+")) == NULL)
                adios (cp, "unable to write");
            chmod (cp, m_gmprot ());

            pc.written = 0;
            pc.mailout = pf;
            if (pop_retr(i, pop_action, &pc) == NOTOK)
                die("%s", response);

            if (fflush (pf))
                adios (cp, "write error on");
            fseek (pf, 0L, SEEK_SET);
	    switch (incerr = scan (pf, msgnum, 0, nfs, width,
			      msgnum == mp->hghmsg + 1 && chgflag,
			      1, NULL, pc.written, noisy, &scanl)) {
	    case SCNEOF:
		printf ("%*d  empty\n", DMAXFOLDER, msgnum);
		break;

	    case SCNFAT:
		trnflag = 0;
		noisy = true;
		/* advise (cp, "unable to read"); already advised */
		break;

	    case SCNERR:
	    case SCNNUM:
		break;

	    case SCNMSG:
	    case SCNENC:
	    default:
		if (aud)
		    fputs (charstring_buffer (scanl), aud);
		if (noisy)
		    fflush (stdout);
		break;
	    }
	    charstring_free (scanl);

            if (ferror(pf) || fclose (pf)) {
                int e = errno;
                (void) m_unlink (cp);
                pop_quit ();
                errno = e;
                adios (cp, "write error on");
            }
            free (cp);

	    if (trnflag && pop_dele (i) == NOTOK)
		die("%s", response);

	    scan_finished();
	}

	if (pop_quit () == NOTOK)
	    die("%s", response);

    } else if (inc_type == INC_FILE && Maildir == NULL) {
        /* Mail from a spool file. */

	scan_detect_mbox_style (in);		/* the MAGIC invocation... */
	hghnum = msgnum = mp->hghmsg;
	for (;;) {
	    charstring_t scanl = NULL;

	    /* create scanline for new message */
	    switch (incerr = scan (in, msgnum + 1, msgnum + 1, nfs, width,
			      msgnum == hghnum && chgflag, 1, NULL, 0L, noisy,
			      &scanl)) {
	    case SCNFAT:
	    case SCNEOF:
		break;

	    case SCNERR:
		if (aud)
		    fputs ("inc aborted!\n", aud);
		inform("aborted!");	/* doesn't clean up locks! */
		break;

	    case SCNNUM:
		inform("BUG in %s, number out of range", invo_name);
		break;

	    default:
		inform("BUG in %s, scan() botch (%d)", invo_name, incerr);
		break;

	    case SCNMSG:
	    case SCNENC:
		/*
		 *  Run the external program hook on the message.
		 */

		(void)snprintf(b, sizeof (b), "%s/%d", maildir_copy, msgnum + 1);
		(void)ext_hook("add-hook", b, NULL);

		if (aud)
		    fputs (charstring_buffer (scanl), aud);
		if (noisy)
		    fflush (stdout);

		msgnum++;
		continue;
	    }
	    charstring_free (scanl);

	    /* If we get here there was some sort of error from scan(),
	     * so stop processing anything more from the spool.
	     */
	    break;
	}

    } else {
        /* Mail from Maildir. */
	char *sp;
	FILE *sf;
	int i;

	hghnum = msgnum = mp->hghmsg;
	for (i = 0; i < num_maildir_entries; i++) {
	    charstring_t scanl = NULL;

	    msgnum++;

	    sp = Maildir[i].filename;
	    cp = mh_xstrdup(m_name (msgnum));
	    pf = NULL;
	    if (!trnflag || link(sp, cp) == -1) {
		static char buf[65536];
		size_t nrd;

		if ((sf = fopen (sp, "r")) == NULL)
	    	    adios (sp, "unable to read for copy");
		if ((pf = fopen (cp, "w+")) == NULL)
		    adios (cp, "unable to write for copy");
		while ((nrd = fread(buf, 1, sizeof(buf), sf)) > 0)
		    if (fwrite(buf, 1, nrd, pf) != nrd)
			break;
		if (ferror(sf) || fflush(pf) || ferror(pf)) {
			int e = errno;
			fclose(pf); fclose(sf); (void) m_unlink(cp);
			errno = e;
			adios(cp, "copy error %s -> %s", sp, cp);
		}
		fclose (sf);
		sf = NULL;
	    }
	    if (pf == NULL && (pf = fopen (cp, "r")) == NULL)
	        adios (cp, "not available");
	    chmod (cp, m_gmprot ());

	    fseek (pf, 0L, SEEK_SET);
	    switch (incerr = scan (pf, msgnum, 0, nfs, width,
			      msgnum == mp->hghmsg + 1 && chgflag,
			      1, NULL, 0, noisy, &scanl)) {
	    case SCNEOF:
		printf ("%*d  empty\n", DMAXFOLDER, msgnum);
		break;

	    case SCNFAT:
		trnflag = 0;
		noisy = true;
		/* advise (cp, "unable to read"); already advised */
		break;

	    case SCNERR:
	    case SCNNUM:
		break;

	    case SCNMSG:
	    case SCNENC:
	    default:
		/*
		 *  Run the external program hook on the message.
		 */

		(void)snprintf(b, sizeof (b), "%s/%d", maildir_copy, msgnum + 1);
		(void)ext_hook("add-hook", b, NULL);

		if (aud)
		    fputs (charstring_buffer (scanl), aud);
		if (noisy)
		    fflush (stdout);
		break;
	    }
	    charstring_free (scanl);

	    if (ferror(pf) || fclose (pf)) {
		int e = errno;
		(void) m_unlink (cp);
		errno = e;
		adios (cp, "write error on");
	    }
	    pf = NULL;
	    free (cp);

	    if (trnflag && m_unlink (sp) == NOTOK)
		adios (sp, "couldn't unlink");
	    free (sp); /* Free Maildir[i]->filename */

	    scan_finished();
	}
	free (Maildir); /* From now on Maildir is just a flag - don't dref! */
    }

    scan_finished ();

    if (incerr < 0) {		/* error */
	if (locked) {
            GETGROUPPRIVS();      /* Be sure we can unlock mail file */
            (void) lkfclosespool (in, newmail); in = NULL;
            DROPGROUPPRIVS();    /* And then return us to normal privileges */
	} else {
	    fclose (in); in = NULL;
	}
	die("failed");
    }

    if (aud)
	fclose (aud);

    if (noisy)
	fflush (stdout);

    if (inc_type == INC_FILE && Maildir == NULL) {
        /* Mail from a spool file;  truncate it. */

	if (trnflag) {
	    if (stat (newmail, &st) != NOTOK && s1.st_mtime != st.st_mtime)
		inform("new messages have arrived!\007");
	    else {
		int newfd;
		if ((newfd = creat (newmail, 0600)) != NOTOK)
		    close (newfd);
		else
		    admonish (newmail, "error zero'ing");
	    }
	} else {
	    if (noisy)
		printf ("%s not zero'd\n", newmail);
	}
    }

    if (msgnum == hghnum) {
	inform("no messages incorporated, continuing...");
    } else {
    	/*
	 * Lock the sequence file now, and loop to set the right flags
	 * in the folder structure
	 */

	struct msgs *mp2;
	int i;

	context_replace (pfolder, folder);	/* update current folder */

	if ((mp2 = folder_read(folder, 1)) == NULL) {
	    inform("Unable to reread folder %s, continuing...", folder);
	    goto skip;
	}

	/*
	 * Shouldn't happen, but just in case ...
	 */

	if (msgnum >= mp2->hghoff
		&& !(mp2 = folder_realloc (mp2, mp2->lowoff, msgnum + 1))) {
	    inform("unable to reallocate folder storage");
	    goto skip;
	}

	if (chgflag)
	    mp2->curmsg = hghnum + 1;
	mp2->hghmsg = msgnum;

	if (mp2->lowmsg == 0)
	    mp2->lowmsg = 1;
	if (chgflag)		/* sigh... */
	    seq_setcur (mp2, mp2->curmsg);

	for (i = hghnum + 1; i <= msgnum; i++) {
	    clear_msg_flags (mp2, i);
	    set_exists (mp2, i);
	    set_unseen (mp2, i);
	}
	mp2->msgflags |= SEQMOD;
	seq_setunseen(mp2, 0);	/* Set the Unseen-Sequence */
	seq_save(mp2);		/* Save the sequence file */
	folder_free(mp2);
    }

skip:
    if (inc_type == INC_FILE && Maildir == NULL) {
        /* Mail from a spool file;  unlock it. */

	if (locked) {
           GETGROUPPRIVS();        /* Be sure we can unlock mail file */
           (void) lkfclosespool (in, newmail); in = NULL;
           DROPGROUPPRIVS();       /* And then return us to normal privileges */
	} else {
	    fclose (in); in = NULL;
	}
    }

    context_save ();		/* save the context file   */
    done (0);
    return 1;
}


static void NORETURN
inc_done (int status)
{
    set_done(exit);
    if (locked)
    {
        GETGROUPPRIVS();
        lkfclosespool(in, newmail);
        DROPGROUPPRIVS();
    }
    exit (status);
}

static int
pop_action(void *closure, char *s)
{
    pop_closure *pc;
    int n;

    pc = closure;
    n = fprintf(pc->mailout, "%s\n", s);
    if (n < 0)
        return NOTOK;
    pc->written += n; /* Count linefeed too. */

    return OK;
}
