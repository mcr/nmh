
/*
 * inc.c -- incorporate messages from a maildrop into a folder
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#ifdef MAILGROUP
/* Revised: Sat Apr 14 17:08:17 PDT 1990 (marvit@hplabs)
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

#include <h/mh.h>
#include <h/utils.h>
#include <fcntl.h>

#ifdef POP
# include <h/dropsbr.h>
# include <h/popsbr.h>
#endif

#include <h/fmt_scan.h>
#include <h/scansbr.h>
#include <h/signals.h>
#include <h/tws.h>
#include <h/mts.h>
#include <errno.h>
#include <signal.h>

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
#define	AUDSW                      0
    { "audit audit-file", 0 },
#define	NAUDSW                     1
    { "noaudit", 0 },
#define	CHGSW                      2
    { "changecur", 0 },
#define	NCHGSW                     3
    { "nochangecur", 0 },
#define	FILESW                     4
    { "file name", 0 },
#define	FORMSW                     5
    { "form formatfile", 0 },
#define	FMTSW                      6
    { "format string", 5 },
#define	HOSTSW                     7
    { "host hostname", POPminc (-4) },
#define	USERSW                     8
    { "user username", POPminc (-4) },
#define	PACKSW                     9
    { "pack file", POPminc (-4) },
#define	NPACKSW                   10
    { "nopack", POPminc (-6) },
#define PORTSW			  11
    { "port name/number", POPminc (-4) },
#define	SILSW                     12
    { "silent", 0 },
#define	NSILSW                    13
    { "nosilent", 0 },
#define	TRNCSW                    14
    { "truncate", 0 },
#define	NTRNCSW                   15
    { "notruncate", 0 },
#define	WIDTHSW                   16
    { "width columns", 0 },
#define VERSIONSW                 17
    { "version", 0 },
#define	HELPSW                    18
    { "help", 0 },
#define SNOOPSW                   19
    { "snoop", -5 },
#define SASLSW                    20
    { "sasl", SASLminc(-4) },
#define SASLMECHSW                21
    { "saslmech", SASLminc(-8) },
#define PROXYSW                   22
    { "proxy command", POPminc(-5) },
    { NULL, 0 }
};

/*
 * flags for the mail source
 */
#define INC_FILE  0
#define INC_POP   1

static int inc_type;
static struct Maildir_entry {
	char *filename;
	time_t mtime;
} *Maildir = NULL;
static int num_maildir_entries = 0;
static int snoop = 0;

#ifdef POP
extern char response[];

static int size;
static long pos;

static int mbx_style = MMDF_FORMAT;
static int pd = NOTOK;
#endif /* POP */

static long start;
static long stop;

static char *packfile = NULL;
static FILE *pf = NULL;

/* This is an attempt to simplify things by putting all the
 * privilege ops into macros.
 * *GROUPPRIVS() is related to handling the setgid MAIL property,
 * and only applies if MAILGROUP is defined.
 * *USERPRIVS() is related to handling the setuid root property,
 * and only applies if POP is defined [why does POP => setuid root?]
 * Basically, SAVEGROUPPRIVS() is called right at the top of main()
 * to initialise things, and then DROPGROUPPRIVS() and GETGROUPPRIVS()
 * do the obvious thing. TRYDROPGROUPPRIVS() has to be safe to call
 * before DROPUSERPRIVS() is called [this is needed because setgid()
 * sets both effective and real uids if euid is root.]
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
static int return_gid;
#ifndef POP
/* easy case; we're not setuid root, so can drop group privs
 * immediately.
 */
#define TRYDROPGROUPPRIVS() DROPGROUPPRIVS()
#else /* POP ie we are setuid root */
#define TRYDROPGROUPPRIVS() \
if (geteuid() != 0) DROPGROUPPRIVS()
#endif
#define DROPGROUPPRIVS() setgid(getgid())
#define GETGROUPPRIVS() setgid(return_gid)
#define SAVEGROUPPRIVS() return_gid = getegid()
#else
/* define *GROUPPRIVS() as null; this avoids having lots of "#ifdef MAILGROUP"s */
#define TRYDROPGROUPPRIVS()
#define DROPGROUPPRIVS()
#define GETGROUPPRIVS()
#define SAVEGROUPPRIVS()
#endif /* not MAILGROUP */

/* these variables have to be globals so that done() can correctly clean up the lockfile */
static int locked = 0;
static char *newmail;
static FILE *in;

/*
 * prototypes
 */
char *map_name(char *);

static void inc_done(int) NORETURN;
#ifdef POP
static int pop_action(char *);
static int pop_pack(char *);
static int map_count(void);
#endif

int
maildir_srt(const void *va, const void *vb)
{
    const struct Maildir_entry *a = va, *b = vb;
    if (a->mtime > b->mtime)
      return 1;
    else if (a->mtime < b->mtime)
      return -1;
    else
      return 0;
}

int
main (int argc, char **argv)
{
    int chgflag = 1, trnflag = 1;
    int noisy = 1, width = 0;
    int hghnum = 0, msgnum = 0;
    int sasl = 0;
    int incerr = 0; /* <0 if inc hits an error which means it should not truncate mailspool */
    char *cp, *maildir = NULL, *folder = NULL;
    char *format = NULL, *form = NULL;
    char *host = NULL, *port = NULL, *user = NULL, *proxy = NULL;
    char *audfile = NULL, *from = NULL, *saslmech = NULL;
    char buf[BUFSIZ], **argp, *nfs, **arguments;
    struct msgs *mp = NULL;
    struct stat st, s1;
    FILE *aud = NULL;
    char b[MAXPATHLEN + 1];
    char *maildir_copy = NULL;	/* copy of mail directory because the static gets overwritten */

#ifdef POP
    int nmsgs, nbytes;
    char *pass = NULL;
    char *MAILHOST_env_variable;
#endif

#ifdef MHE
    FILE *mhe = NULL;
#endif

    done=inc_done;

/* absolutely the first thing we do is save our privileges,
 * and drop them if we can.
 */
    SAVEGROUPPRIVS();
    TRYDROPGROUPPRIVS();

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    mts_init (invo_name);
    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

#ifdef POP
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

    if ((cp = getenv ("MHPOPDEBUG")) && *cp)
	snoop++;
#endif /* POP */

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
	    case AMBIGSW: 
		ambigsw (cp, switches);
		done (1);
	    case UNKWNSW: 
		adios (NULL, "-%s unknown", cp);

	    case HELPSW: 
		snprintf (buf, sizeof(buf), "%s [+folder] [switches]", invo_name);
		print_help (buf, switches, 1);
		done (1);
	    case VERSIONSW:
		print_version(invo_name);
		done (1);

	    case AUDSW: 
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		audfile = getcpy (m_maildir (cp));
		continue;
	    case NAUDSW: 
		audfile = NULL;
		continue;

	    case CHGSW: 
		chgflag++;
		continue;
	    case NCHGSW: 
		chgflag = 0;
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
		    adios (NULL, "missing argument to %s", argp[-2]);
		from = path (cp, TFILE);

		/*
		 * If the truncate file is in default state,
		 * change to not truncate.
		 */
		if (trnflag == 1)
		    trnflag = 0;
		continue;

	    case SILSW: 
		noisy = 0;
		continue;
	    case NSILSW: 
		noisy++;
		continue;

	    case FORMSW: 
		if (!(form = *argp++) || *form == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		format = NULL;
		continue;
	    case FMTSW: 
		if (!(format = *argp++) || *format == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		form = NULL;
		continue;

	    case WIDTHSW: 
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		width = atoi (cp);
		continue;

	    case HOSTSW:
		if (!(host = *argp++) || *host == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;

	    case PORTSW:
		if (!(host = *argp++) || *port == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;

	    case USERSW:
		if (!(user = *argp++) || *user == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;

	    case PACKSW:
#ifndef	POP
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
#else /* POP */
		if (!(packfile = *argp++) || *packfile == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
#endif /* POP */
		continue;
	    case NPACKSW:
#ifdef POP
		packfile = NULL;
#endif /* POP */
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
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		adios (NULL, "only one folder at a time!");
	    else
		folder = pluspath (cp);
	} else {
	    adios (NULL, "usage: %s [+folder] [switches]", invo_name);
	}
    }

    /* NOTE: above this point you should use TRYDROPGROUPPRIVS(),
     * not DROPGROUPPRIVS().
     */
#ifdef POP
    if (host && !*host)
	host = NULL;
#endif /* POP */

    /* guarantee dropping group priveleges; we might not have done so earlier */
    DROPGROUPPRIVS();

    /*
     * Where are we getting the new mail?
     */
    if (from)
	inc_type = INC_FILE;
#ifdef POP
    else if (host)
	inc_type = INC_POP;
#endif
    else
	inc_type = INC_FILE;

#ifdef POP
    /*
     * Are we getting the mail from
     * a POP server?
     */
    if (inc_type == INC_POP) {
	if (user == NULL)
	    user = getusername ();
	if (sasl)
	    pass = getusername ();
	else
	    ruserpass (host, &user, &pass);

	/*
	 * initialize POP connection
	 */
	if (pop_init (host, port, user, pass, proxy, snoop, sasl,
		      saslmech) == NOTOK)
	    adios (NULL, "%s", response);

	/* Check if there are any messages */
	if (pop_stat (&nmsgs, &nbytes) == NOTOK)
	    adios (NULL, "%s", response);

	if (nmsgs == 0) {
	    pop_quit();
	    adios (NULL, "no mail to incorporate");
	}
    }
#endif /* POP */

    /*
     * We will get the mail from a file
     * (typically the standard maildrop)
     */

    if (inc_type == INC_FILE) {
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
	    adios (NULL, "no mail to incorporate");
 	if (s1.st_mode & S_IFDIR) {
 	    DIR *md;
 	    struct dirent *de;
 	    struct stat ms;
 	    int i;
 	    i = 0;
 	    cp = concat (newmail, "/new", NULL);
 	    if ((md = opendir(cp)) == NULL)
 		adios (NULL, "unable to open %s", cp);
 	    while ((de = readdir (md)) != NULL) {
 		if (de->d_name[0] == '.')
 		    continue;
 		if (i >= num_maildir_entries) {
 		    if ((Maildir = realloc(Maildir, sizeof(*Maildir) * (2*i+16))) == NULL)
 			adios(NULL, "not enough memory for %d messages", 2*i+16);
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
 		adios (NULL, "unable to open %s", cp);
 	    while ((de = readdir (md)) != NULL) {
 		if (de->d_name[0] == '.')
 		    continue;
 		if (i >= num_maildir_entries) {
 		    if ((Maildir = realloc(Maildir, sizeof(*Maildir) * (2*i+16))) == NULL)
 			adios(NULL, "not enough memory for %d messages", 2*i+16);
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
 	        adios (NULL, "no mail to incorporate");
 	    num_maildir_entries = i;
 	    qsort (Maildir, num_maildir_entries, sizeof(*Maildir), maildir_srt);
 	}

	if ((cp = strdup(newmail)) == (char *)0)
	    adios (NULL, "error allocating memory to copy newmail");

	newmail = cp;
    }

#ifdef POP
    /* skip the folder setup */
    if ((inc_type == INC_POP) && packfile)
	goto go_to_it;
#endif /* POP */

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (!folder)
	folder = getfolder (0);
    maildir = m_maildir (folder);

    if ((maildir_copy = strdup(maildir)) == (char *)0)
        adios (maildir, "error allocating memory to copy maildir");

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
    if (!(mp = folder_read (folder)))
	adios (NULL, "unable to read folder %s", folder);

#ifdef POP
go_to_it:
#endif /* POP */

    if (inc_type == INC_FILE && Maildir == NULL) {
	if (access (newmail, W_OK) != NOTOK) {
	    locked++;
	    if (trnflag) {
		SIGNAL (SIGHUP, SIG_IGN);
		SIGNAL (SIGINT, SIG_IGN);
		SIGNAL (SIGQUIT, SIG_IGN);
		SIGNAL (SIGTERM, SIG_IGN);
	    }

            GETGROUPPRIVS();       /* Reset gid to lock mail file */
            in = lkfopen (newmail, "r");
            DROPGROUPPRIVS();
            if (in == NULL)
		adios (NULL, "unable to lock and fopen %s", newmail);
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
	    advise (NULL, "Creating Receive-Audit: %s", audfile);
	if ((aud = fopen (audfile, "a")) == NULL)
	    adios (audfile, "unable to append to");
	else if (i == NOTOK)
	    chmod (audfile, m_gmprot ());

#ifdef POP
	fprintf (aud, from ? "<<inc>> %s -ms %s\n"
		 : host ? "<<inc>> %s -host %s -user %s\n"
		 : "<<inc>> %s\n",
		 dtimenow (0), from ? from : host, user);
#else /* POP */
	fprintf (aud, from ? "<<inc>> %s  -ms %s\n" : "<<inc>> %s\n",
		 dtimenow (0), from);
#endif /* POP */
    }

#ifdef MHE
    if (context_find ("mhe")) {
	int i;
	cp = concat (maildir, "/++", NULL);
	i = stat (cp, &st);
	if ((mhe = fopen (cp, "a")) == NULL)
	    admonish (cp, "unable to append to");
	else
	    if (i == NOTOK)
		chmod (cp, m_gmprot ());
	free (cp);
    }
#endif /* MHE */

    /* Get new format string */
    nfs = new_fs (form, format, FORMAT);

    if (noisy) {
	printf ("Incorporating new mail into %s...\n\n", folder);
	fflush (stdout);
    }

#ifdef POP
    /*
     * Get the mail from a POP server
     */
    if (inc_type == INC_POP) {
	int i;
	if (packfile) {
	    packfile = path (packfile, TFILE);
	    if (stat (packfile, &st) == NOTOK) {
		if (errno != ENOENT)
		    adios (packfile, "error on file");
		cp = concat ("Create file \"", packfile, "\"? ", NULL);
		if (noisy && !getanswer (cp))
		    done (1);
		free (cp);
	    }
	    msgnum = map_count ();
	    if ((pd = mbx_open (packfile, mbx_style, getuid(), getgid(), m_gmprot()))
		== NOTOK)
		adios (packfile, "unable to open");
	    if ((pf = fdopen (pd, "w+")) == NULL)
		adios (NULL, "unable to fdopen %s", packfile);
	} else {
	    hghnum = msgnum = mp->hghmsg;
	    /*
	     * Check if we have enough message space for all the new
	     * messages.  If not, then realloc the folder and add enough
	     * space for all new messages plus 10 additional slots.
	     */
	    if (mp->hghmsg + nmsgs >= mp->hghoff
		&& !(mp = folder_realloc (mp, mp->lowoff, mp->hghmsg + nmsgs + 10)))
		adios (NULL, "unable to allocate folder storage");
	}

	for (i = 1; i <= nmsgs; i++) {
	    msgnum++;
	    if (packfile) {
		fseek (pf, 0L, SEEK_CUR);
		pos = ftell (pf);
		size = 0;
		fwrite (mmdlm1, 1, strlen (mmdlm1), pf);
		start = ftell (pf);

		if (pop_retr (i, pop_pack) == NOTOK)
		    adios (NULL, "%s", response);

		fseek (pf, 0L, SEEK_CUR);
		stop = ftell (pf);
		if (fflush (pf))
		    adios (packfile, "write error on");
		fseek (pf, start, SEEK_SET);
	    } else {
		cp = getcpy (m_name (msgnum));
		if ((pf = fopen (cp, "w+")) == NULL)
		    adios (cp, "unable to write");
		chmod (cp, m_gmprot ());
		start = stop = 0L;

		if (pop_retr (i, pop_action) == NOTOK)
		    adios (NULL, "%s", response);

		if (fflush (pf))
		    adios (cp, "write error on");
		fseek (pf, 0L, SEEK_SET);
	    }
	    switch (incerr = scan (pf, msgnum, 0, nfs, width,
			      packfile ? 0 : msgnum == mp->hghmsg + 1 && chgflag,
			      1, NULL, stop - start, noisy)) {
	    case SCNEOF: 
		printf ("%*d  empty\n", DMAXFOLDER, msgnum);
		break;

	    case SCNFAT:
		trnflag = 0;
		noisy++;
		/* advise (cp, "unable to read"); already advised */
		/* fall thru */

	    case SCNERR:
	    case SCNNUM: 
		break;

	    case SCNMSG: 
	    case SCNENC:
	    default: 
		if (aud)
		    fputs (scanl, aud);
# ifdef MHE
		if (mhe)
		    fputs (scanl, mhe);
# endif /* MHE */
		if (noisy)
		    fflush (stdout);
		if (!packfile) {
		    clear_msg_flags (mp, msgnum);
		    set_exists (mp, msgnum);
		    set_unseen (mp, msgnum);
		    mp->msgflags |= SEQMOD;
		}
		break;
	    }
	    if (packfile) {
		fseek (pf, stop, SEEK_SET);
		fwrite (mmdlm2, 1, strlen (mmdlm2), pf);
		if (fflush (pf) || ferror (pf)) {
		    int e = errno;
		    pop_quit ();
		    errno = e;
		    adios (packfile, "write error on");
		}
		map_write (packfile, pd, 0, 0L, start, stop, pos, size, noisy);
	    } else {
		if (ferror(pf) || fclose (pf)) {
		    int e = errno;
		    unlink (cp);
		    pop_quit ();
		    errno = e;
		    adios (cp, "write error on");
		}
		free (cp);
	    }

	    if (trnflag && pop_dele (i) == NOTOK)
		adios (NULL, "%s", response);
	}

	if (pop_quit () == NOTOK)
	    adios (NULL, "%s", response);
	if (packfile) {
	    mbx_close (packfile, pd);
	    pd = NOTOK;
	}
    }
#endif /* POP */

    /*
     * Get the mail from file (usually mail spool)
     */
    if (inc_type == INC_FILE && Maildir == NULL) {
	m_unknown (in);		/* the MAGIC invocation... */
	hghnum = msgnum = mp->hghmsg;
	for (;;) {
	    /*
	     * Check if we need to allocate more space for message status.
	     * If so, then add space for an additional 100 messages.
	     */
	    if (msgnum >= mp->hghoff
		&& !(mp = folder_realloc (mp, mp->lowoff, mp->hghoff + 100))) {
		advise (NULL, "unable to allocate folder storage");
		incerr = NOTOK;
		break;
	    }

#if 0
	    /* copy file from spool to tmp file */
	    tmpfilenam = m_scratch ("", invo_name);
	    if ((fd = creat (tmpfilenam, m_gmprot ())) == NOTOK)
		adios (tmpfilenam, "unable to create");
	    chmod (tmpfilenam, m_gmprot ());
	    if (!(in2 = fdopen (fd, "r+")))
		adios (tmpfilenam, "unable to access");
	    cpymsg (in, in2);

	    /* link message into folder */
	    newmsg = folder_addmsg(mp, tmpfilenam);
#endif
	    /* create scanline for new message */
	    switch (incerr = scan (in, msgnum + 1, msgnum + 1, nfs, width,
			      msgnum == hghnum && chgflag, 1, NULL, 0L, noisy)) {
	    case SCNFAT:
	    case SCNEOF: 
		break;

	    case SCNERR:
		if (aud)
		    fputs ("inc aborted!\n", aud);
		advise (NULL, "aborted!");	/* doesn't clean up locks! */
		break;

	    case SCNNUM: 
		advise (NULL, "BUG in %s, number out of range", invo_name);
		break;

	    default: 
		advise (NULL, "BUG in %s, scan() botch (%d)", invo_name, incerr);
		break;

	    case SCNMSG:
	    case SCNENC:
		/*
		 *  Run the external program hook on the message.
		 */

		(void)snprintf(b, sizeof (b), "%s/%d", maildir_copy, msgnum + 1);
		(void)ext_hook("add-hook", b, (char *)0);

		if (aud)
		    fputs (scanl, aud);
#ifdef MHE
		if (mhe)
		    fputs (scanl, mhe);
#endif /* MHE */
		if (noisy)
		    fflush (stdout);

		msgnum++;
		mp->hghmsg++;
		mp->nummsg++;
		if (mp->lowmsg == 0) mp->lowmsg = 1;

		clear_msg_flags (mp, msgnum);
		set_exists (mp, msgnum);
		set_unseen (mp, msgnum);
		mp->msgflags |= SEQMOD;
		continue;
	    }
	    /* If we get here there was some sort of error from scan(),
	     * so stop processing anything more from the spool.
	     */
	    break;
	}
    } else if (inc_type == INC_FILE) { /* Maildir inbox to process */
	char *sp;
	FILE *sf;
	int i;

	hghnum = msgnum = mp->hghmsg;
	for (i = 0; i < num_maildir_entries; i++) {
	    msgnum++;
	    /*
	     * Check if we need to allocate more space for message status.
	     * If so, then add space for an additional 100 messages.
	     */
	    if (msgnum >= mp->hghoff
		&& !(mp = folder_realloc (mp, mp->lowoff, mp->hghoff + 100))) {
		advise (NULL, "unable to allocate folder storage");
		incerr = NOTOK;
		break;
	    }

	    sp = Maildir[i].filename;
	    cp = getcpy (m_name (msgnum));
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
			fclose(pf); fclose(sf); unlink(cp);
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
			      1, NULL, stop - start, noisy)) {
	    case SCNEOF: 
		printf ("%*d  empty\n", DMAXFOLDER, msgnum);
		break;

	    case SCNFAT:
		trnflag = 0;
		noisy++;
		/* advise (cp, "unable to read"); already advised */
		/* fall thru */

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
		(void)ext_hook("add-hook", b, (char *)0);

		if (aud)
		    fputs (scanl, aud);
# ifdef MHE
		if (mhe)
		    fputs (scanl, mhe);
# endif /* MHE */
		if (noisy)
		    fflush (stdout);
		if (!packfile) {
		    clear_msg_flags (mp, msgnum);
		    set_exists (mp, msgnum);
		    set_unseen (mp, msgnum);
		    mp->msgflags |= SEQMOD;
		}
		break;
	    }
	    if (ferror(pf) || fclose (pf)) {
		int e = errno;
		unlink (cp);
		errno = e;
		adios (cp, "write error on");
	    }
	    pf = NULL;
	    free (cp);

	    if (trnflag && unlink (sp) == NOTOK)
		adios (sp, "couldn't unlink");
	    free (sp); /* Free Maildir[i]->filename */
	}
	free (Maildir); /* From now on Maildir is just a flag - don't dref! */
    }

    if (incerr < 0) {		/* error */
	if (locked) {
            GETGROUPPRIVS();      /* Be sure we can unlock mail file */
            (void) lkfclose (in, newmail); in = NULL;
            DROPGROUPPRIVS();    /* And then return us to normal privileges */
	} else {
	    fclose (in); in = NULL;
	}
	adios (NULL, "failed");
    }

    if (aud)
	fclose (aud);

#ifdef MHE
    if (mhe)
	fclose (mhe);
#endif /* MHE */

    if (noisy)
	fflush (stdout);

#ifdef POP
    if ((inc_type == INC_POP) && packfile)
	done (0);
#endif /* POP */

    /*
     * truncate file we are incorporating from
     */
    if (inc_type == INC_FILE && Maildir == NULL) {
	if (trnflag) {
	    if (stat (newmail, &st) != NOTOK && s1.st_mtime != st.st_mtime)
		advise (NULL, "new messages have arrived!\007");
	    else {
		int newfd;
		if ((newfd = creat (newmail, 0600)) != NOTOK)
		    close (newfd);
		else
		    admonish (newmail, "error zero'ing");
		unlink(map_name(newmail));
	    }
	} else {
	    if (noisy)
		printf ("%s not zero'd\n", newmail);
	}
    }

    if (msgnum == hghnum) {
	admonish (NULL, "no messages incorporated");
    } else {
	context_replace (pfolder, folder);	/* update current folder */
	if (chgflag)
	    mp->curmsg = hghnum + 1;
	mp->hghmsg = msgnum;
	if (mp->lowmsg == 0)
	    mp->lowmsg = 1;
	if (chgflag)		/* sigh... */
	    seq_setcur (mp, mp->curmsg);
    }

    /*
     * unlock the mail spool
     */
    if (inc_type == INC_FILE && Maildir == NULL) {
	if (locked) {
           GETGROUPPRIVS();        /* Be sure we can unlock mail file */
           (void) lkfclose (in, newmail); in = NULL;
           DROPGROUPPRIVS();       /* And then return us to normal privileges */
	} else {
	    fclose (in); in = NULL;
	}
    }

    seq_setunseen (mp, 0);	/* set the Unseen-Sequence */
    seq_save (mp);		/* synchronize sequences   */
    context_save ();		/* save the context file   */
    done (0);
    return 1;
}


#if 0

/*
 * Copy message message from spool into
 * temporary file.  Massage the "From " line
 * while copying.
 */

cpymsg (FILE *in, FILE *out)
{
    int state;
    char *tmpbuf, name[NAMESZ];

    for (;;) {
	state = m_getfld (state, name, tmpbuf, rlwidth, in);
	switch (state) {
	case FLD:
	case FLDPLUS:
	    break;
	case BODY:
	    break;
	case LENERR:
	case FMTERR:
	    break;
	case FILEEOF:
	    break;
	default:
	}
    }
}
#endif /* if 0 */


static void
inc_done (int status)
{
#ifdef POP
    if (packfile && pd != NOTOK)
	mbx_close (packfile, pd);
#endif /* POP */
    if (locked)
    {
        GETGROUPPRIVS();
        lkfclose(in, newmail);
        DROPGROUPPRIVS();
    }
    exit (status);
}

#ifdef POP
static int
pop_action (char *s)
{
    fprintf (pf, "%s\n", s);
    stop += strlen (s) + 1;
    return 0;  /* Is return value used?  This was missing before 1999-07-15. */
}

static int
pop_pack (char *s)
{
    int j;
    char buffer[BUFSIZ];

    snprintf (buffer, sizeof(buffer), "%s\n", s);
    for (j = 0; (j = stringdex (mmdlm1, buffer)) >= 0; buffer[j]++)
	continue;
    for (j = 0; (j = stringdex (mmdlm2, buffer)) >= 0; buffer[j]++)
	continue;
    fputs (buffer, pf);
    size += strlen (buffer) + 1;
    return 0;  /* Is return value used?  This was missing before 1999-07-15. */
}

static int
map_count (void)
{
    int md;
    char *cp;
    struct drop d;
    struct stat st;

    if (stat (packfile, &st) == NOTOK)
	return 0;
    if ((md = open (cp = map_name (packfile), O_RDONLY)) == NOTOK
	    || map_chk (cp, md, &d, (long) st.st_size, 1)) {
	if (md != NOTOK)
	    close (md);
	return 0;
    }
    close (md);
    return (d.d_id);
}
#endif /* POP */
