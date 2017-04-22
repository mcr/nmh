/*
 * slocal.c -- asynchronously filter and deliver new mail
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/*
 *  Under sendmail, users should add the line
 *
 * 	"| /usr/local/nmh/lib/slocal"
 *
 *  to their $HOME/.forward file.
 *
 */

/* Changed to use getutent() and friends.  Assumes that when getutent() exists,
 * a number of other things also exist.  Please check.
 * Ruud de Rooij <ruud@ruud.org>  Sun, 28 May 2000 17:28:55 +0200
 */

#include <h/mh.h>
#include <h/dropsbr.h>
#include <h/rcvmail.h>
#include <h/signals.h>
#include <setjmp.h>
#include <h/tws.h>
#include <h/mts.h>
#include <h/utils.h>

#include <pwd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

/* Hopefully, grp.h declares initgroups().  If we run into a platform
   where it doesn't, we could consider declaring it here as well. */
#include <grp.h>

/* This define is needed for Berkeley db v2 and above to
 * make the header file expose the 'historical' ndbm APIs.
 * We define it unconditionally because this is simple and
 * harmless.
 */
#define DB_DBM_HSEARCH 1
#ifdef DB_DBM_HSEARCH
#endif /* Use DB_DBM_HSEARCH to prevent warning from gcc -Wunused-macros. */
#ifdef NDBM_HEADER
#include NDBM_HEADER
#endif

#ifdef HAVE_GETUTXENT
#include <utmpx.h>
#endif /* HAVE_GETUTXENT */

#define SLOCAL_SWITCHES \
    X("addr address", 0, ADDRSW) \
    X("user name", 0, USERSW) \
    X("file file", 0, FILESW) \
    X("sender address", 0, SENDERSW) \
    X("mailbox file", 0, MAILBOXSW) \
    X("home directory", -4, HOMESW) \
    X("info data", 0, INFOSW) \
    X("maildelivery file", 0, MAILSW) \
    X("verbose", 0, VERBSW) \
    X("noverbose", 0, NVERBSW) \
    X("suppressdup", 0, SUPPRESSDUP) \
    X("nosuppressdup", 0, NSUPPRESSDUP) \
    X("debug", 0, DEBUGSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(SLOCAL);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(SLOCAL, switches);
#undef X

static int globbed = 0;		/* have we built "vars" table yet?        */
static int parsed = 0;		/* have we built header field table yet   */
static int utmped = 0;		/* have we scanned umtp(x) file yet       */
static int suppressdup = 0;	/* are we suppressing duplicate messages? */

static int verbose = 0;
static int debug = 0;

static char *addr = NULL;
static char *user = NULL;
static char *info = NULL;
static char *file = NULL;
static char *sender = NULL;
static char *envelope = NULL;	/* envelope information ("From " line)  */
static char *mbox = NULL;
static char *home = NULL;

static struct passwd *pw;	/* passwd file entry */

static char ddate[BUFSIZ];	/* record the delivery date */
struct tws *now;

static jmp_buf myctx;

/* flags for pair->p_flags */
#define	P_NIL  0x00
#define	P_ADR  0x01	/* field is address     */
#define	P_HID  0x02	/* special (fake) field */
#define	P_CHK  0x04

struct pair {
    char *p_name;
    char *p_value;
    char  p_flags;
};

#define	NVEC 100

/*
 * Lookup table for matching fields and patterns
 * in messages.  The rest of the table is added
 * when the message is parsed.
 */
static struct pair hdrs[NVEC + 1] = {
    { "source",          NULL, P_HID },
    { "addr",            NULL, P_HID },
    { "Return-Path",     NULL, P_ADR },
    { "Reply-To",        NULL, P_ADR },
    { "From",            NULL, P_ADR },
    { "Sender",          NULL, P_ADR },
    { "To",              NULL, P_ADR },
    { "cc",              NULL, P_ADR },
    { "Resent-Reply-To", NULL, P_ADR },
    { "Resent-From",     NULL, P_ADR },
    { "Resent-Sender",   NULL, P_ADR },
    { "Resent-To",       NULL, P_ADR },
    { "Resent-cc",       NULL, P_ADR },
    { NULL, NULL, 0 }
};

/*
 * The list of builtin variables to expand in a string
 * before it is executed by the "pipe" or "qpipe" action.
 */
static struct pair vars[] = {
    { "sender",   NULL, P_NIL },
    { "address",  NULL, P_NIL },
    { "size",     NULL, P_NIL },
    { "reply-to", NULL, P_CHK },
    { "info",     NULL, P_NIL },
    { NULL, NULL, 0 }
};

extern char **environ;

/*
 * static prototypes
 */
static int localmail (int, char *);
static int usr_delivery (int, char *, int);
static int split (char *, char **);
static int parse (int);
static void expand (char *, char *, int);
static void glob (int);
static struct pair *lookup (struct pair *, char *);
static int logged_in (void);
static int timely (char *, char *);
static int usr_file (int, char *, int);
static int usr_pipe (int, char *, char *, char **, int);
static int usr_folder (int, char *);
static void alrmser (int);
static void get_sender (char *, char **);
static int copy_message (int, char *, int);
static void verbose_printf (char *fmt, ...);
static void adorn (char *, char *, ...);
static void debug_printf (char *fmt, ...);
static int suppress_duplicates (int, char *);
static char *trim (char *);


int
main (int argc, char **argv)
{
    int fd, status;
    FILE *fp;
    char *cp, *mdlvr = NULL, buf[BUFSIZ];
    char mailbox[BUFSIZ], tmpfil[BUFSIZ];
    char **argp, **arguments;

    if (nmh_init(argv[0], 0 /* use context_foil() */)) { return 1; }

    mts_init ();
    arguments = getarguments (invo_name, argc, argv, 0);
    argp = arguments;

    /* Parse arguments */
    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [switches]", invo_name);
		    print_help (buf, switches, 0);
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

		case ADDRSW: 
		    if (!(addr = *argp++))/* allow -xyz arguments */
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case INFOSW: 
		    if (!(info = *argp++))/* allow -xyz arguments */
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case USERSW: 
		    if (!(user = *argp++))/* allow -xyz arguments */
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case FILESW: 
		    if (!(file = *argp++) || *file == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case SENDERSW: 
		    if (!(sender = *argp++))/* allow -xyz arguments */
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case MAILBOXSW: 
		    if (!(mbox = *argp++) || *mbox == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case HOMESW: 
		    if (!(home = *argp++) || *home == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case MAILSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if (mdlvr)
			adios (NULL, "only one maildelivery file at a time!");
		    mdlvr = cp;
		    continue;

		case VERBSW: 
		    verbose++;
		    continue;
		case NVERBSW: 
		    verbose = 0;
		    continue;

		case SUPPRESSDUP:
		    suppressdup++;
		    continue;
		case NSUPPRESSDUP:
		    suppressdup = 0;
		    continue;
		case DEBUGSW: 
		    debug++;
		    continue;
	    }
	} else {
		adios (NULL, "only switch arguments are supported");
	}
    }

    if (addr == NULL)
	addr = getusername ();
    if (user == NULL) {
	user = getusername ();
    }
    if ((pw = getpwnam (user)) == NULL)
	adios (NULL, "no such local user as %s", user);

    if (chdir (pw->pw_dir) == -1)
	if (chdir ("/") < 0) {
	    advise ("/", "chdir");
	}
    umask (0077);

    if (geteuid() == 0) {
	if (setgid (pw->pw_gid) != 0) {
	    adios ("setgid", "unable to set group to %ld", (long) pw->pw_gid);
	}
	initgroups (pw->pw_name, pw->pw_gid);
	if (setuid (pw->pw_uid) != 0) {
	    adios ("setuid", "unable to set user to %ld", (long) pw->pw_uid);
	}
    }

    if (info == NULL)
	info = "";

    setbuf (stdin, NULL);

    /* Record the delivery time */
    if ((now = dlocaltimenow ()) == NULL)
	adios (NULL, "unable to ascertain local time");
    snprintf (ddate, sizeof(ddate), "Delivery-Date: %s\n", dtimenow (0));

    /*
     * Copy the message to a temporary file
     */
    if (file) {
	int tempfd;

	/* getting message from file */
	if ((tempfd = open (file, O_RDONLY)) == -1)
	    adios (file, "unable to open");
	if (debug)
	    debug_printf ("retrieving message from file \"%s\"\n", file);
	if ((fd = copy_message (tempfd, tmpfil, 1)) == -1)
	    adios(NULL, "unable to create temporary file in %s",
                  get_temp_dir());
	close (tempfd);
    } else {
	/* getting message from stdin */
	if (debug)
	    debug_printf ("retrieving message from stdin\n");
	if ((fd = copy_message (fileno (stdin), tmpfil, 1)) == -1)
	    adios(NULL, "unable to create temporary file in %s",
                  get_temp_dir());
    }

    if (debug)
	debug_printf ("temporary file=\"%s\"\n", tmpfil);

    /* Delete the temp file now or a copy of every single message passed through
       slocal will be left in the /tmp directory until deleted manually!  This
       unlink() used to be under an 'else' of the 'if (debug)' above, but since
       some people like to always run slocal with -debug and log the results,
       the /tmp directory would get choked over time.  Of course, now that we
       always delete the temp file, the "temporary file=" message above is
       somewhat pointless -- someone watching debug output wouldn't have a
       chance to 'tail -f' or 'ln' the temp file before it's unlinked.  The best
       thing would be to delay this unlink() until later if debug == 1, but I'll
       leave that for someone who cares about the temp-file-accessing
       functionality (they'll have to watch out for cases where we adios()). */
    (void) m_unlink (tmpfil);

    if (!(fp = fdopen (fd, "r+")))
	adios (NULL, "unable to access temporary file");

    /*
     * If no sender given, extract it
     * from envelope information.  */
    if (sender == NULL)
	get_sender (envelope, &sender);

    if (mbox == NULL) {
	snprintf (mailbox, sizeof(mailbox), "%s/%s",
		mmdfldir[0] ? mmdfldir : pw->pw_dir,
		mmdflfil[0] ? mmdflfil : pw->pw_name);
	mbox = mailbox;
    }
    if (home == NULL)
	home = pw->pw_dir;

    if (debug) {
	debug_printf ("addr=\"%s\"\n", trim(addr));
	debug_printf ("user=\"%s\"\n", trim(user));
	debug_printf ("info=\"%s\"\n", trim(info));
	debug_printf ("sender=\"%s\"\n", trim(sender));
	debug_printf ("envelope=\"%s\"\n", envelope ? trim(envelope) : "");
	debug_printf ("mbox=\"%s\"\n", trim(mbox));
	debug_printf ("home=\"%s\"\n", trim(home));
	debug_printf ("ddate=\"%s\"\n", trim(ddate));
	debug_printf ("now=%02d:%02d\n\n", now->tw_hour, now->tw_min);
    }

    /* deliver the message */
    status = localmail (fd, mdlvr);

    done (status != -1 ? RCV_MOK : RCV_MBX);
    return 1;
}


/*
 * Main routine for delivering message.
 */

static int
localmail (int fd, char *mdlvr)
{
    /* check if this message is a duplicate */
    if (suppressdup &&
        suppress_duplicates(fd, mdlvr ? mdlvr : ".maildelivery") == DONE)
	return 0;

    /* delivery according to personal Maildelivery file */
    if (usr_delivery (fd, mdlvr ? mdlvr : ".maildelivery", 0) != -1)
	return 0;

    /* delivery according to global Maildelivery file */
    if (usr_delivery (fd, maildelivery, 1) != -1)
	return 0;

    if (verbose)
	verbose_printf ("(delivering to standard mail spool)\n");

    /* last resort - deliver to standard mail spool */
    return usr_file (fd, mbox, MBOX_FORMAT);
}


#define	matches(a,b) (stringdex (b, a) >= 0)

/*
 * Parse the delivery file, and process incoming message.
 */

static int
usr_delivery (int fd, char *delivery, int su)
{
    int i, accept, status=1, won, vecp, next;
    char *field, *pattern, *action, *result, *string;
    char buffer[BUFSIZ], tmpbuf[BUFSIZ];
    char *vec[NVEC];
    struct stat st;
    struct pair *p;
    FILE *fp;

    /* open the delivery file */
    if ((fp = fopen (delivery, "r")) == NULL)
	return -1;

    /* check if delivery file has bad ownership or permissions */
    if (fstat (fileno (fp), &st) == -1
	    || (st.st_uid != 0 && (su || st.st_uid != pw->pw_uid))
	    || st.st_mode & (S_IWGRP|S_IWOTH)) {
	if (verbose) {
	    verbose_printf ("WARNING: %s has bad ownership/modes (su=%d,uid=%d,owner=%d,mode=0%o)\n",
		    delivery, su, (int) pw->pw_uid, (int) st.st_uid, (int) st.st_mode);
	}
	return -1;
    }

    won = 0;
    next = 1;

    /* read and process delivery file */
    while (fgets (buffer, sizeof(buffer), fp)) {
	/* skip comments and empty lines */
	if (*buffer == '#' || *buffer == '\n')
	    continue;

        trim_suffix_c(buffer, '\n');

	/* split buffer into fields */
	vecp = split (buffer, vec);

	/* check for too few fields */
	if (vecp < 5) {
	    if (debug)
		debug_printf ("WARNING: entry with only %d fields, skipping.\n", vecp);
	    continue;
	}

	if (debug) {
	    for (i = 0; vec[i]; i++)
		debug_printf ("vec[%d]: \"%s\"\n", i, trim(vec[i]));
	}

	field   = vec[0];
	pattern = vec[1];
	action  = vec[2];
	result  = vec[3];
	string  = vec[4];

	/* find out how to perform the action */
	switch (result[0]) {
	    case 'N':
	    case 'n':
		/*
		 * If previous condition failed, don't
		 * do this - else fall through
		 */
 		if (!next)
		    continue;
		/* FALLTHRU */

	    case '?': 
		/*
		 * If already delivered, skip this action.  Else
		 * consider delivered if action is successful.
		 */
		if (won)
		    continue;
		/* FALLTHRU */

	    case 'A': 
	    case 'a': 
		/*
		 * Take action, and consider delivered if
		 * action is successful.
		 */
		accept = 1;
		break;

	    case 'R': 
	    case 'r': 
	    default: 
		/*
		 * Take action, but don't consider delivered, even
		 * if action is successful
		 */
		accept = 0;
		break;
	}

	if (vecp > 5) {
	    if (!strcasecmp (vec[5], "select")) {
		if (logged_in () != -1)
		    continue;
		if (vecp > 7 && timely (vec[6], vec[7]) == -1)
		    continue;
	    }
	}

	/* check if the field matches */
	switch (*field) {
	    case '*': 
	    /* always matches */
		break;

	    case 'd': 
	    /*
	     * "default" matches only if the message hasn't
	     * been delivered yet.
	     */
		if (!strcasecmp (field, "default")) {
		    if (won)
			continue;
		    break;
		}
		/* FALLTHRU */

	    default: 
		/* parse message and build lookup table */
		if (!parsed && parse (fd) == -1) {
		    fclose (fp);
		    return -1;
		}
		/*
		 * find header field in lookup table, and
		 * see if the pattern matches.
		 */
		if ((p = lookup (hdrs, field)) && (p->p_value != NULL)
			&& matches (p->p_value, pattern)) {
		    next = 1;
		} else {
		    next = 0;
		    continue;
		}
		break;
	}

	/* find out the action to perform */
	switch (*action) {
	    case 'q':
		/* deliver to quoted pipe */
		if (strcasecmp (action, "qpipe"))
		    continue;
		/* FALLTHRU */
	    case '^':
		expand (tmpbuf, string, fd);
		if (split (tmpbuf, vec) < 1)
		    continue;
		status = usr_pipe (fd, tmpbuf, vec[0], vec, 0);
		break;

	    case 'p': 
		/* deliver to pipe */
		if (strcasecmp (action, "pipe"))
		    continue;
		/* FALLTHRU */
	    case '|': 
		vec[2] = "sh";
		vec[3] = "-c";
		expand (tmpbuf, string, fd);
		vec[4] = tmpbuf;
		vec[5] = NULL;
		status = usr_pipe (fd, tmpbuf, "/bin/sh", vec + 2, 0);
		break;

	    case 'f': 
		/* mbox format */
		if (!strcasecmp (action, "file")) {
		    status = usr_file (fd, string, MBOX_FORMAT);
		    break;
		}
		/* deliver to nmh folder */
		else if (strcasecmp (action, "folder"))
		    continue;
		/* FALLTHRU */
	    case '+':
		status = usr_folder (fd, string);
		break;

	    case 'm':
		/* mmdf format */
		if (!strcasecmp (action, "mmdf")) {
		    status = usr_file (fd, string, MMDF_FORMAT);
		    break;
		}
		/* mbox format */
		else if (strcasecmp (action, "mbox"))
		    continue;
		/* FALLTHRU */

	    case '>': 
		/* mbox format */
		status = usr_file (fd, string, MBOX_FORMAT);
		break;

	    case 'd': 
		/* ignore message */
		if (strcasecmp (action, "destroy"))
		    continue;
		status = 0;
		break;
	}

	if (status) next = 0;	/* action failed, mark for 'N' result */

	if (accept && status == 0)
	    won++;
    }

    fclose (fp);
    return (won ? 0 : -1);
}


#define	QUOTE	'\\'

/*
 * Split buffer into fields (delimited by whitespace or
 * comma's).  Return the number of fields found.
 */

static int
split (char *cp, char **vec)
{
    int i;
    char *s;

    s = cp;

    /* split into a maximum of NVEC fields */
    for (i = 0; i <= NVEC;) {
	vec[i] = NULL;

	/* zap any whitespace and comma's */
	while (isspace ((unsigned char) *s) || *s == ',')
	    *s++ = 0;

	/* end of buffer, time to leave */
	if (*s == 0)
	    break;

	/* get double quote text as a single field */
	if (*s == '"') {
	    for (vec[i++] = ++s; *s && *s != '"'; s++) {
		/*
		 * Check for escaped double quote.  We need
		 * to shift the string to remove slash.
		 */
		if (*s == QUOTE) {
		    if (*++s == '"')
			strcpy (s - 1, s);
		    s--;
		}
	    }
	    if (*s == '"')	/* zap trailing double quote */
		*s++ = 0;
	    continue;
	}

	if (*s == QUOTE && *++s != '"')
	    s--;
	vec[i++] = s++;

	/* move forward to next field delimiter */
	while (*s && !isspace ((unsigned char) *s) && *s != ',')
	    s++;
    }
    vec[i] = NULL;

    return i;
}


/*
 * Parse the headers of a message, and build the
 * lookup table for matching fields and patterns.
 */

static int
parse (int fd)
{
    int i, state;
    int fd1;
    char *cp, *dp, *lp;
    char name[NAMESZ], field[BUFSIZ];
    struct pair *p, *q;
    FILE  *in;
    m_getfld_state_t gstate = 0;

    if (parsed++)
	return 0;

    /* get a new FILE pointer to message */
    if ((fd1 = dup (fd)) == -1)
	return -1;
    if ((in = fdopen (fd1, "r")) == NULL) {
	close (fd1);
	return -1;
    }
    rewind (in);

    /* add special entries to lookup table */
    if ((p = lookup (hdrs, "source")))
	p->p_value = getcpy (sender);
    if ((p = lookup (hdrs, "addr")))
	p->p_value = getcpy (addr);

    /*
     * Scan the headers of the message and build
     * a lookup table.
     */
    for (i = 0;;) {
	int fieldsz = sizeof field;
	switch (state = m_getfld (&gstate, name, field, &fieldsz, in)) {
	    case FLD: 
	    case FLDPLUS: 
		lp = mh_xstrdup(field);
		while (state == FLDPLUS) {
		    fieldsz = sizeof field;
		    state = m_getfld (&gstate, name, field, &fieldsz, in);
		    lp = add (field, lp);
		}
		for (p = hdrs; p->p_name; p++) {
		    if (!strcasecmp (p->p_name, name)) {
			if (!(p->p_flags & P_HID)) {
			    if ((cp = p->p_value)) {
				if (p->p_flags & P_ADR) {
				    dp = cp + strlen (cp) - 1;
				    if (*dp == '\n')
					*dp = 0;
				    cp = add (",\n\t", cp);
				} else {
				    cp = add ("\t", cp);
				}
			    }
			    p->p_value = add (lp, cp);
			}
			free (lp);
			break;
		    }
		}
		if (p->p_name == NULL && i < NVEC) {
		    p->p_name = mh_xstrdup(name);
		    p->p_value = lp;
		    p->p_flags = P_NIL;
		    p++, i++;
		    p->p_name = NULL;
		}
		continue;

	    case BODY: 
	    case FILEEOF: 
		break;

	    case LENERR: 
	    case FMTERR: 
		inform("format error in message");
		break;

	    default: 
		inform("internal error in m_getfld");
		fclose (in);
		return -1;
	}
	break;
    }
    m_getfld_state_destroy (&gstate);
    fclose (in);

    if ((p = lookup (vars, "reply-to"))) {
	if ((q = lookup (hdrs, "reply-to")) == NULL || q->p_value == NULL)
	    q = lookup (hdrs, "from");
	p->p_value = getcpy (q ? q->p_value : "");
	p->p_flags &= ~P_CHK;
	if (debug)
	    debug_printf ("vars[%d]: name=\"%s\" value=\"%s\"\n",
		    p - vars, p->p_name, trim(p->p_value));
    }
    if (debug) {
	for (p = hdrs; p->p_name; p++)
	    debug_printf ("hdrs[%d]: name=\"%s\" value=\"%s\"\n",
		p - hdrs, p->p_name, p->p_value ? trim(p->p_value) : "");
    }

    return 0;
}


#define	LPAREN	'('
#define	RPAREN	')'

/*
 * Expand any builtin variables such as $(sender),
 * $(address), etc., in a string.
 */

static void
expand (char *s1, char *s2, int fd)
{
    char c, *cp;
    struct pair *p;

    if (!globbed)
	glob (fd);

    while ((c = *s2++)) {
	if (c != '$' || *s2 != LPAREN) {
	    *s1++ = c;
	} else {
	    for (cp = ++s2; *s2 && *s2 != RPAREN; s2++)
		continue;
	    if (*s2 != RPAREN) {
		s2 = --cp;
		continue;
	    }
	    *s2++ = 0;
	    if ((p = lookup (vars, cp))) {
		if (!parsed && (p->p_flags & P_CHK))
		    parse (fd);

		strcpy (s1, p->p_value);
		s1 += strlen (s1);
	    }
	}
    }
    *s1 = 0;
}


/*
 * Fill in the information missing from the "vars"
 * table, which is necessary to expand any builtin
 * variables in the string for a "pipe" or "qpipe"
 * action.
 */

static void
glob (int fd)
{
    char buffer[BUFSIZ];
    struct stat st;
    struct pair *p;

    if (globbed++)
	return;

    if ((p = lookup (vars, "sender")))
	p->p_value = getcpy (sender);
    if ((p = lookup (vars, "address")))
	p->p_value = getcpy (addr);
    if ((p = lookup (vars, "size"))) {
	snprintf (buffer, sizeof(buffer), "%d",
		fstat (fd, &st) != -1 ? (int) st.st_size : 0);
	p->p_value = mh_xstrdup(buffer);
    }
    if ((p = lookup (vars, "info")))
	p->p_value = getcpy (info);

    if (debug) {
	for (p = vars; p->p_name; p++)
	    debug_printf ("vars[%d]: name=\"%s\" value=\"%s\"\n",
		    p - vars, p->p_name, trim(p->p_value));
    }
}


/*
 * Find a matching name in a lookup table.  If found,
 * return the "pairs" entry, else return NULL.
 */

static struct pair *
lookup (struct pair *pairs, char *key)
{
    for (; pairs->p_name; pairs++)
	if (!strcasecmp (pairs->p_name, key))
	    return pairs;

    return NULL;
}


/*
 * Check utmp(x) file to see if user is currently
 * logged in.
 */

static int
logged_in (void)
{
#if HAVE_GETUTXENT
    struct utmpx *utp;

    if (utmped)
        return utmped;

    setutxent();

    while ((utp = getutxent()) != NULL) {
        if ( utp->ut_type == USER_PROCESS && utp->ut_user[0] != 0
                && strncmp (user, utp->ut_user, sizeof(utp->ut_user)) == 0) {
            if (debug)
                continue;
            endutxent();
            return (utmped = DONE);
        }
    }

    endutxent();
#endif /* HAVE_GETUTXENT */
    return (utmped = NOTOK);
}

#define	check(t,a,b)		if (t < a || t > b) return -1
#define	cmpar(h1,m1,h2,m2)	if (h1 < h2 || (h1 == h2 && m1 < m2)) return 0

static int
timely (char *t1, char *t2)
{
    int t1hours, t1mins, t2hours, t2mins;

    if (sscanf (t1, "%d:%d", &t1hours, &t1mins) != 2)
	return -1;
    check (t1hours, 0, 23);
    check (t1mins, 0, 59);

    if (sscanf (t2, "%d:%d", &t2hours, &t2mins) != 2)
	return -1;
    check (t2hours, 0, 23);
    check (t2mins, 0, 59);

    cmpar (now->tw_hour, now->tw_min, t1hours, t1mins);
    cmpar (t2hours, t2mins, now->tw_hour, now->tw_min);

    return -1;
}


/*
 * Deliver message by appending to a file.
 */

static int
usr_file (int fd, char *mailbox, int mbx_style)
{
    int	md, mapping;

    if (verbose)
	verbose_printf ("delivering to file \"%s\"", mailbox);

    if (mbx_style == MBOX_FORMAT) {
	if (verbose)
	    verbose_printf (" (mbox style)");
	mapping = 0;
    } else {
	if (verbose)
	    verbose_printf (" (mmdf style)");
	mapping = 1;
    }

    /* open and lock the file */
    if ((md = mbx_open (mailbox, mbx_style, pw->pw_uid, pw->pw_gid, m_gmprot())) == -1) {
	if (verbose)
	    adorn ("", "unable to open:");
	return -1;
    }

    lseek (fd, (off_t) 0, SEEK_SET);

    /* append message to file */
    if (mbx_copy (mailbox, mbx_style, md, fd, mapping, NULL, verbose) == -1) {
	if (verbose)
	    adorn ("", "error writing to:");
	return -1;
    }

    /* close and unlock file */
    if (mbx_close (mailbox, md) == NOTOK) {
        if (verbose)
	    adorn ("", "error closing:");
        return -1;
    }

    if (verbose)
	verbose_printf (", success.\n");
    return 0;
}


/*
 * Deliver message to a nmh folder.
 */

static int
usr_folder (int fd, char *string)
{
    int status;
    char folder[BUFSIZ], *vec[3];

    /* get folder name ready */
    if (*string == '+')
	strncpy(folder, string, sizeof(folder));
    else
	snprintf(folder, sizeof(folder), "+%s", string);

    if (verbose)
	verbose_printf ("delivering to folder \"%s\"", folder + 1);

    vec[0] = "rcvstore";
    vec[1] = folder;
    vec[2] = NULL;

    /* use rcvstore to put message in folder */
    status = usr_pipe (fd, "rcvstore", rcvstoreproc, vec, 1);

    return status;
}

/*
 * Deliver message to a process.
 */

static int
usr_pipe (int fd_arg, char *cmd, char *pgm, char **vec, int suppress)
{
    volatile int fd = fd_arg;
    pid_t child_id;
    int i, bytes, seconds, status;
    struct stat st;

    if (verbose && !suppress)
	verbose_printf ("delivering to pipe \"%s\"", cmd);

    lseek (fd, (off_t) 0, SEEK_SET);

    for (i = 0; (child_id = fork()) == -1 && i < 5; i++)
	sleep (5);

    switch (child_id) {
	case -1: 
	    /* fork error */
	    if (verbose)
		adorn ("fork", "unable to");
	    return -1;

	case 0: 
	    /* child process */
	    if (fd != 0)
		dup2 (fd, 0);
	    if (freopen ("/dev/null", "w", stdout) == NULL) {
		advise ("stdout", "freopen");
	    }
	    if (freopen ("/dev/null", "w", stderr) == NULL) {
		advise ("stderr", "freopen");
	    }
	    if (fd != 3)
		dup2 (fd, 3);
	    closefds (4);

#ifdef TIOCNOTTY
	    if ((fd = open ("/dev/tty", O_RDWR)) != -1) {
		ioctl (fd, TIOCNOTTY, NULL);
		close (fd);
	    }
#endif /* TIOCNOTTY */

	    setpgid ((pid_t) 0, getpid ());	/* put in own process group */

	    *environ = NULL;
	    m_putenv ("USER", pw->pw_name);
	    m_putenv ("HOME", pw->pw_dir);
	    m_putenv ("SHELL", pw->pw_shell);

	    execvp (pgm, vec);
	    _exit (-1);

	default: 
	    /* parent process */
	    if (! setjmp (myctx)) {
		SIGNAL (SIGALRM, alrmser);
		bytes = fstat (fd, &st) != -1 ? (int) st.st_size : 100;

		/* amount of time to wait depends on message size */
		if (bytes <= 100) {
		    /* give at least 5 minutes */
		    seconds = 300;
		} else if (bytes >= 90000) {
		    /* a half hour is long enough */
		    seconds = 1800;
		} else {
		    seconds = (bytes / 60) + 300;
		}
		alarm ((unsigned int) seconds);
		status = pidwait (child_id, 0);
		alarm (0);

		if (verbose) {
		    if (status == 0)
			verbose_printf (", success.\n");
		    else
			if ((status & 0xff00) == 0xff00)
			    verbose_printf (", system error\n");
			else
			    pidstatus (status, stdout, ", failed");
		}
		return (status == 0 ? 0 : -1);
	    }
            /*
             * Ruthlessly kill the child and anything
             * else in its process group.
             */
            killpg(child_id, SIGKILL);
            if (verbose)
                verbose_printf (", timed-out; terminated\n");
            return -1;
    }
}


static void
alrmser (int i)
{
    NMH_UNUSED (i);

    longjmp (myctx, DONE);
}


/*
 * Get the `sender' from the envelope
 * information ("From " line).
 */

static void
get_sender (char *envelope, char **sender)
{
    int i;
    char *cp;
    char buffer[BUFSIZ];

    if (envelope == NULL) {
	*sender = mh_xstrdup("");
	return;
    }

    i = LEN("From ");
    strncpy (buffer, envelope + i, sizeof(buffer));
    if ((cp = strchr(buffer, '\n'))) {
	*cp = 0;
	cp -= 24;
	if (cp < buffer)
	    cp = buffer;
    } else {
	cp = buffer;
    }
    *cp = 0;

    for (cp = buffer + strlen (buffer) - 1; cp >= buffer; cp--)
	if (isspace ((unsigned char) *cp))
	    *cp = 0;
	else
	    break;
    *sender = mh_xstrdup(buffer);
}


/*
 * Copy message into a temporary file.
 * While copying, it will do some header processing
 * including the extraction of the envelope information.
 */

static int
copy_message (int qd, char *tmpfil, int fold)
{
    int i, first = 1, fd1, fd2;
    char buffer[BUFSIZ];
    FILE *qfp, *ffp;
    char *tfile = NULL;

    tfile = m_mktemp2(NULL, invo_name, &fd1, NULL);
    if (tfile == NULL) return -1;
    strncpy (tmpfil, tfile, BUFSIZ);

    if (!fold) {
	while ((i = read (qd, buffer, sizeof(buffer))) > 0)
	    if (write (fd1, buffer, i) != i) {
you_lose:
		close (fd1);
		(void) m_unlink (tmpfil);
		return -1;
	    }
	if (i == -1)
	    goto you_lose;
	lseek (fd1, (off_t) 0, SEEK_SET);
	return fd1;
    }

    /* dup the fd for incoming message */
    if ((fd2 = dup (qd)) == -1) {
	close (fd1);
	return -1;
    }

    /* now create a FILE pointer for it */
    if ((qfp = fdopen (fd2, "r")) == NULL) {
	close (fd1);
	close (fd2);
	return -1;
    }

    /* dup the fd for temporary file */
    if ((fd2 = dup (fd1)) == -1) {
	close (fd1);
	fclose (qfp);
	return -1;
    }

    /* now create a FILE pointer for it */
    if ((ffp = fdopen (fd2, "r+")) == NULL) {
	close (fd1);
	close (fd2);
	fclose (qfp);
	return -1;
    }

    /*
     * copy message into temporary file
     * and massage the headers.  Save
     * a copy of the "From " line for later.
     */
    while (fgets (buffer, sizeof(buffer), qfp)) {
	if (first) {
	    first = 0;
	    if (has_prefix(buffer, "From ")) {
		/* get copy of envelope information ("From " line) */
		envelope = mh_xstrdup(buffer);

		/* Put the delivery date in message */
		fputs (ddate, ffp);
		if (ferror (ffp))
		    goto fputs_error;

		continue;
	    }
	}

	fputs (buffer, ffp);
	if (ferror (ffp))
	    goto fputs_error;
    }

    fclose (ffp);
    if (ferror (qfp)) {
	close (fd1);
	fclose (qfp);
	return -1;
    }
    fclose (qfp);
    lseek (fd1, (off_t) 0, SEEK_SET);
    return fd1;


fputs_error:
    close (fd1);
    fclose (ffp);
    fclose (qfp);
    return -1;
}

/*
 * Trim strings for pretty printing of debugging output
 */

static char *
trim (char *cp)
{
    char buffer[BUFSIZ*4];
    char *bp, *sp;

    if (cp == NULL)
	return NULL;

    /* copy string into temp buffer */
    strncpy (buffer, cp, sizeof(buffer));
    bp = buffer;

    /* skip over leading whitespace */
    while (isspace((unsigned char) *bp))
	bp++;

    /* start at the end and zap trailing whitespace */
    for (sp = bp + strlen(bp) - 1; sp >= bp; sp--) {
	if (isspace((unsigned char) *sp))
	    *sp = 0;
	else
	    break;
    }

    /* replace remaining whitespace with spaces */
    for (sp = bp; *sp; sp++)
	if (isspace((unsigned char) *sp))
	    *sp = ' ';

    /* now return a copy */
    return mh_xstrdup(bp);
}

/*
 * Function for printing `verbose' messages.
 */

static void
verbose_printf (char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    fflush (stdout);	/* now flush output */
}


/*
 * Function for printing `verbose' delivery
 * error messages.
 */

static void
adorn (char *what, char *fmt, ...)
{
    va_list ap;
    int eindex;
    char *s;

    eindex = errno;	/* save the errno */
    printf(", ");

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    if (what) {
	if (*what)
	    printf(" %s: ", what);
	if ((s = strerror (eindex)))
	    fputs(s, stdout);
	else
	    printf("Error %d", eindex);
    }

    putchar('\n');
    fflush (stdout);
}


/*
 * Function for printing `debug' messages.
 */

static void
debug_printf (char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end(ap);
}


/*
 * Check ndbm/db file(s) to see if the Message-Id of this
 * message matches the Message-Id of a previous message,
 * so we can discard it.  If it doesn't match, we add the
 * Message-Id of this message to the ndbm/db file.
 */
static int
suppress_duplicates (int fd, char *file)
{
    int	fd1, lockfd, state, result;
    char *cp, buf[BUFSIZ], name[NAMESZ];
    datum key, value;
    DBM *db;
    FILE *in;
    m_getfld_state_t gstate = 0;

    if ((fd1 = dup (fd)) == -1)
	return -1;
    if (!(in = fdopen (fd1, "r"))) {
	close (fd1);
	return -1;
    }
    rewind (in);

    for (;;) {
        int failed_to_lock = 0;
	int bufsz = sizeof buf;
	state = m_getfld (&gstate, name, buf, &bufsz, in);
	switch (state) {
	    case FLD:
	    case FLDPLUS:
		/* Search for the message ID */
		if (strcasecmp (name, "Message-ID")) {
		    while (state == FLDPLUS) {
			bufsz = sizeof buf;
			state = m_getfld (&gstate, name, buf, &bufsz, in);
		    }
		    continue;
		}

		cp = mh_xstrdup(buf);
		while (state == FLDPLUS) {
		    bufsz = sizeof buf;
		    state = m_getfld (&gstate, name, buf, &bufsz, in);
		    cp = add (buf, cp);
		}
		key.dptr = trimcpy (cp);
		key.dsize = strlen (key.dptr) + 1;
		free (cp);
		cp = key.dptr;

		if (!(db = dbm_open (file, O_RDWR | O_CREAT, 0600))) {
		    advise (file, "unable to perform dbm_open on");
		    free (cp);
		    fclose (in);
		    return -1;
		}
		/*
		 * Since it is difficult to portable lock a ndbm file,
		 * we will open and lock the Maildelivery file instead.
		 * This will fail if your Maildelivery file doesn't
		 * exist.
		 */
		if ((lockfd = lkopendata(file, O_RDWR, 0, &failed_to_lock))
		    == -1) {
		    advise (file, "unable to perform file locking on");
		    free (cp);
		    fclose (in);
		    return -1;
		}
		value = dbm_fetch (db, key);
		if (value.dptr) {
		    if (verbose)
		        verbose_printf ("Message-ID: %s\n            already received on %s",
				 cp, value.dptr);
		    result = DONE;
		} else {
		    value.dptr  = ddate + sizeof("Delivery-Date:");
		    value.dsize = strlen(value.dptr) + 1;
		    if (dbm_store (db, key, value, DBM_INSERT))
			advise (file, "possibly corrupt file");
		    result = 0;
		}

		dbm_close (db);
		lkclosedata(lockfd, file);
		free (cp);
		fclose (in);
		return result;

	   case BODY:
	   case FILEEOF:
		break;

	   case LENERR:
	   case FMTERR:
	   default:
		break;
	}

	break;
    }
    m_getfld_state_destroy (&gstate);

    fclose (in);
    return 0;
}
