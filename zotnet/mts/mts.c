
/*
 * mts.c -- definitions for the mail transport system
 *
 * $Id$
 */

#include "h/mh.h"   /* for snprintf() */
#include <h/nmh.h>

#define nmhetcdir(file) NMHETCDIR#file

#include <ctype.h>
#include <stdio.h>
#include <mts.h>
#include <pwd.h>
#include <netdb.h>

#ifdef HAVE_SYS_UTSNAME_H
# include <sys/utsname.h>
#endif

#define	NOTOK   (-1)
#define	OK        0

extern int errno;

/*
 * static prototypes
 */
static char *tailor_value (char *);
static void getuserinfo (void);

/*
 * *mmdfldir and *uucpldir are the maildrop directories.  If maildrops
 * are kept in the user's home directory, then these should be empty
 * strings.  In this case, the appropriate ...lfil array should contain
 * the name of the file in the user's home directory.  Usually, this is
 * something like ".mail".
 */

/*
 * nmh mail transport interface customization file
 */
static char *mtsconf = nmhetcdir(/mts.conf);

static char *localname   = "";
static char *localdomain = "";
static char *systemname  = "";

char *mmdfldir = MAILSPOOL;
char *mmdflfil = "";
char *uucpldir = "/usr/spool/mail";
char *uucplfil = "";

char *mmdlm1 = "\001\001\001\001\n";
char *mmdlm2 = "\001\001\001\001\n";

/* Cache the username and fullname of the user */
static char username[BUFSIZ];
static char fullname[BUFSIZ];

/* variables for username masquerading */
int MMailids = 0;  /* used from post.c as well as here */
static char *mmailid = "0";


/*
 * MTS specific variables
 */
#if defined(SENDMTS) || defined(SMTPMTS)
char *hostable = nmhetcdir(/hosts);
char *sendmail = SENDMAILPATH;
#endif

/*
 * SMTP/POP stuff
 */
char *clientname = NULL;
char *servers    = "localhost \01localnet";
char *pophost    = "";

/*
 * BBoards-specific variables
 */
char *bb_domain = "";


/*
 * POP BBoards-specific variables
 */
#ifdef	BPOP
char *popbbhost = "";
char *popbbuser = "";
char *popbblist = nmhetcdir(/hosts.popbb);
#endif /* BPOP */

/*
 * Global MailDelivery file
 */
char *maildelivery = nmhetcdir(/maildelivery);


/*
 * Aliasing Facility (doesn't belong here)
 */
int Everyone = NOTOK;
static char *everyone = "-1";
char *NoShell = "";

/*
 * Customize the MTS settings for nmh by adjusting
 * the file mts.conf in the nmh etc directory.
 */

struct bind {
    char *keyword;
    char **value;
};

static struct bind binds[] = {
    { "localname", &localname },
    { "localdomain", &localdomain },
    { "systemname", &systemname },
    { "mmdfldir", &mmdfldir },
    { "mmdflfil", &mmdflfil },
    { "uucpldir", &uucpldir },
    { "uucplfil", &uucplfil },
    { "mmdelim1", &mmdlm1 },
    { "mmdelim2", &mmdlm2 },
    { "mmailid", &mmailid },

#if defined(SENDMTS) || defined(SMTPMTS)
    { "hostable", &hostable },
#endif

#ifdef SENDMTS
    { "sendmail", &sendmail },
#endif

    { "clientname",  &clientname },
    { "servers", &servers },
    { "pophost", &pophost },
    { "bbdomain", &bb_domain },

#ifdef BPOP
    { "popbbhost", &popbbhost },
    { "popbbuser", &popbbuser },
    { "popbblist", &popbblist },
#endif

#ifdef NNTP
    { "nntphost", &popbbhost },
#endif

    { "maildelivery", &maildelivery },
    { "everyone", &everyone },
    { "noshell", &NoShell },
    { NULL, NULL }
};


/*
 * Read the configuration file for the nmh interface
 * to the mail transport system (MTS).
 */

void
mts_init (char *name)
{
    char *bp, *cp, buffer[BUFSIZ];
    struct bind *b;
    FILE *fp;
    static int inited = 0;

    if (inited++ || (fp = fopen (mtsconf, "r")) == NULL)
	return;

    while (fgets (buffer, sizeof(buffer), fp)) {
	if (!(cp = strchr(buffer, '\n')))
	    break;
	*cp = 0;
	if (*buffer == '#' || *buffer == '\0')
	    continue;
	if (!(bp = strchr(buffer, ':')))
	    break;
	*bp++ = 0;
	while (isspace (*bp))
	    *bp++ = 0;

	for (b = binds; b->keyword; b++)
	    if (!strcmp (buffer, b->keyword))
		break;
	if (b->keyword && (cp = tailor_value (bp)))
	    *b->value = cp;
    }

    fclose (fp);
    MMailids = atoi (mmailid);
    Everyone = atoi (everyone);
}


#define	QUOTE	'\\'

/*
 * Convert escaped values, malloc some new space,
 * and copy string to malloc'ed memory.
 */

static char *
tailor_value (char *s)
{
    int i, r;
    char *bp;
    char buffer[BUFSIZ];
    size_t len;

    for (bp = buffer; *s; bp++, s++) {
	if (*s != QUOTE) {
	    *bp = *s;
	} else {
	    switch (*++s) {
		case 'b': *bp = '\b'; break;
		case 'f': *bp = '\f'; break;
		case 'n': *bp = '\n'; break;
		case 't': *bp = '\t'; break;

		case 0: s--;
		case QUOTE: 
		    *bp = QUOTE;
		    break;

		default: 
		    if (!isdigit (*s)) {
			*bp++ = QUOTE;
			*bp = *s;
		    }
		    r = *s != '0' ? 10 : 8;
		    for (i = 0; isdigit (*s); s++)
			i = i * r + *s - '0';
		    s--;
		    *bp = toascii (i);
		    break;
	    }
	}
    }
    *bp = 0;

    len = strlen (buffer) + 1;
    if ((bp = malloc (len)))
	memcpy (bp, buffer, len);

    return bp;
}

/*
 * Get the fully qualified name of the local host.
 */

char *
LocalName (void)
{
    static char buffer[BUFSIZ] = "";
    struct hostent *hp;

#ifdef HAVE_UNAME
    struct utsname name;
#endif

    /* check if we have cached the local name */
    if (buffer[0])
	return buffer;

    mts_init ("mts");

    /* check if the mts.conf file specifies a "localname" */
    if (*localname) {
	strncpy (buffer, localname, sizeof(buffer));
    } else {
#ifdef HAVE_UNAME
	/* first get our local name */
	uname (&name);
	strncpy (buffer, name.nodename, sizeof(buffer));
#else
	/* first get our local name */
	gethostname (buffer, sizeof(buffer));
#endif
#ifdef HAVE_SETHOSTENT
	sethostent (1);
#endif 
	/* now fully qualify our name */
	if ((hp = gethostbyname (buffer)))
	    strncpy (buffer, hp->h_name, sizeof(buffer));
    }

    /*
     * If the mts.conf file specifies a "localdomain",
     * we append that now.  This should rarely be needed.
     */
    if (*localdomain) {
	strcat (buffer, ".");
	strcat (buffer, localdomain);
    }

    return buffer;
}


/*
 * This is only for UUCP mail.  It gets the hostname
 * as part of the UUCP "domain".
 */

char *
SystemName (void)
{
    static char buffer[BUFSIZ] = "";

#ifdef HAVE_UNAME
    struct utsname name;
#endif

    /* check if we have cached the system name */
    if (buffer[0])
	return buffer;

    mts_init ("mts");

    /* check if mts.conf file specifies a "systemname" */
    if (*systemname) {
	strncpy (buffer, systemname, sizeof(buffer));
	return buffer;
    }

#ifdef HAVE_UNAME
    uname (&name);
    strncpy (buffer, name.nodename, sizeof(buffer));
#else
    gethostname (buffer, sizeof(buffer));
#endif

    return buffer;
}


/*
 * Get the username of current user
 */

char *
getusername (void)
{
    if (username[0] == '\0')
	getuserinfo();

    return username;
}


/*
 * Get full name of current user (typically from GECOS
 * field of password file).
 */

char *
getfullname (void)
{
    if (username[0] == '\0')
	getuserinfo();

    return fullname;
}


/*
 * Find the user's username and full name, and cache them.
 * Also, handle "mmailid" username masquerading controlled from the GECOS field
 * of the passwd file. 
 */

static void
getuserinfo (void)
{
    register char *cp, *np;
    register struct passwd *pw;

#ifdef KPOP
    uid_t uid;

    uid = getuid ();
    if (uid == geteuid () && (cp = getenv ("USER")) != NULL
	&& (pw = getpwnam (cp)) != NULL)
      strncpy (username, cp, sizeof(username));
    else if ((pw = getpwuid (uid)) == NULL
	     || pw->pw_name == NULL
	     || *pw->pw_name == '\0') {
#else /* KPOP */
    if ((pw = getpwuid (getuid ())) == NULL
	    || pw->pw_name == NULL
	    || *pw->pw_name == '\0') {
#endif /* KPOP */

	strncpy (username, "unknown", sizeof(username));
	snprintf (fullname, sizeof(fullname), "The Unknown User-ID (%d)",
		(int) getuid ());
	return;
    }

    np = pw->pw_gecos;

    /*
     * Do mmailid (username masquerading) processing.  The GECOS
     * field should have the form "Full Name <fakeusername>".  For instance,
     * "Dan Harkless <Dan.Harkless>".  Naturally, you'll want your MTA to have
     * an alias (e.g. in /etc/aliases) from "fakeusername" to your account name.
     */
#ifndef	GCOS_HACK
    /* What is this code here for?  As of 2000-01-25, GCOS_HACK doesn't appear
       anywhere else in nmh.  -- Dan Harkless <dan-nmh@dilvish.speed.net> */
    for (cp = fullname; *np && *np != (MMailids ? '<' : ','); *cp++ = *np++)
	continue;
#else
    for (cp = fullname; *np && *np != (MMailids ? '<' : ','); ) {
	if (*np == '&')	{	/* blech! */
	    strcpy (cp, pw->pw_name);
	    *cp = toupper(*cp);
	    while (*cp)
		cp++;
	    np++;
	} else {
	    *cp++ = *np++;
	}
    }
#endif

    *cp = '\0';
    if (MMailids) {
	if (*np)
	    np++;
	for (cp = username; *np && *np != '>'; *cp++ = *np++)
	    continue;
	*cp = '\0';
    }
    if (MMailids == 0 || *np == '\0')
	strncpy (username, pw->pw_name, sizeof(username));

    if ((cp = getenv ("SIGNATURE")) && *cp)
	strncpy (fullname, cp, sizeof(fullname));

    if (strchr(fullname, '.')) {		/*  quote any .'s */
	char tmp[BUFSIZ];

	/* should quote "'s too */
	snprintf (tmp, sizeof(tmp), "\"%s\"", fullname);
	strncpy (fullname, tmp, sizeof(fullname));
    }

    return;
}
