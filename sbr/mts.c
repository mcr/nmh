
/*
 * mts.c -- definitions for the mail transport system
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>   /* for snprintf() */
#include <h/nmh.h>
#include <h/utils.h>

#define nmhetcdir(file) NMHETCDIR#file

#include <ctype.h>
#include <stdio.h>
#include <h/mts.h>
#include <pwd.h>
#include <sys/socket.h>
#include <netdb.h>

/*
 * static prototypes
 */
static char *tailor_value (unsigned char *);
static void getuserinfo (void);
static const char *get_mtsconf_pathname(void);
static const char *get_mtsuserconf_pathname(void);
static void mts_read_conf_file (FILE *fp);

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

/* Cache the username, fullname, and mailbox of the user */
static char username[BUFSIZ];
static char fullname[BUFSIZ];
static char localmbox[BUFSIZ];

/*
 * MTS specific variables
 */
static char *sm_method = "smtp";
int  sm_mts    = MTS_SMTP;
char *sendmail = SENDMAILPATH;

/*
 * SMTP/POP stuff
 */
char *clientname = NULL;
char *servers    = "localhost";
char *pophost    = "";

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
    { "mts",      &sm_method },
    { "sendmail", &sendmail  },
    { "clientname",  &clientname },
    { "servers", &servers },
    { "pophost", &pophost },

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
    const char *cp;
    FILE *fp;
    static int inited = 0;
    NMH_UNUSED (name);

    if (inited++ || (fp = fopen (get_mtsconf_pathname(), "r")) == NULL)
	return;
    mts_read_conf_file(fp);
    fclose (fp);

    cp = get_mtsuserconf_pathname();
    if (cp != NULL &&
            ((fp = fopen (get_mtsuserconf_pathname(), "r")) != NULL)) {
        mts_read_conf_file(fp);
        fclose (fp);
    }

    Everyone = atoi (everyone);

    if (strcmp(sm_method, "smtp") == 0)
        sm_mts = MTS_SMTP;
    else if (strcmp(sm_method, "sendmail") == 0)
        sm_mts = MTS_SENDMAIL;
    else {
        advise(NULL, "unsupported \"mts\" value in mts.conf: %s", sm_method);
        sm_mts = MTS_SMTP;
    }
}


#define	QUOTE	'\\'

/*
 * Convert escaped values, malloc some new space,
 * and copy string to malloc'ed memory.
 */

static char *
tailor_value (unsigned char *s)
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
    bp = mh_xmalloc (len);
    memcpy (bp, buffer, len);

    return bp;
}

/*
 * Get the fully qualified name of the local host.
 *
 * If flag is 0, then use anything out of mts.conf (like localname).
 * If flag is 1, then only use the "proper" local hostname.
 */

char *
LocalName (int flag)
{
    static char buffer0[BUFSIZ] = "";
    static char buffer1[BUFSIZ] = "";
    static char *buffer[] = { buffer0, buffer1 };
    char *buf;
    struct addrinfo hints, *res;

    if (flag < 0 || flag > 1)
    	return NULL;

    buf = buffer[flag];

    /* check if we have cached the local name */
    if (buf[0])
	return buf;

    mts_init ("mts");

    /* check if the mts.conf file specifies a "localname" */
    if (*localname && flag == 0) {
	strncpy (buf, localname, sizeof(buffer0));
    } else {
	memset(buf, 0, sizeof(buffer0));
	/* first get our local name */
	gethostname (buf, sizeof(buffer0) - 1);
	/* now fully qualify our name */

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = PF_UNSPEC;
	if (getaddrinfo(buf, NULL, &hints, &res) == 0) {
	    strncpy(buf, res->ai_canonname, sizeof(buffer0) - 1);
	    freeaddrinfo(res);
	}
    }

    /*
     * If the mts.conf file specifies a "localdomain",
     * we append that now.  This should rarely be needed.
     */
    if (*localdomain) {
	strcat (buf, ".");
	strcat (buf, localdomain);
    }

    return buf;
}


/*
 * This is only for UUCP mail.  It gets the hostname
 * as part of the UUCP "domain".
 */

char *
SystemName (void)
{
    static char buffer[BUFSIZ] = "";

    /* check if we have cached the system name */
    if (buffer[0])
	return buffer;

    mts_init ("mts");

    /* check if mts.conf file specifies a "systemname" */
    if (*systemname) {
	strncpy (buffer, systemname, sizeof(buffer));
	return buffer;
    }

    gethostname (buffer, sizeof(buffer));

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
 * Get the full local mailbox name.  This is in the form:
 *
 * User Name <user@name.com>
 */

char *
getlocalmbox (void)
{
    if (username[0] == '\0')
    	getuserinfo();

    return localmbox;
}

/*
 * Find the user's username and full name, and cache them.
 */

static void
getuserinfo (void)
{
    register unsigned char *cp;
    register char *np;
    register struct passwd *pw;

    if ((pw = getpwuid (getuid ())) == NULL
	    || pw->pw_name == NULL
	    || *pw->pw_name == '\0') {
	strncpy (username, "unknown", sizeof(username));
	snprintf (fullname, sizeof(fullname), "The Unknown User-ID (%d)",
		(int) getuid ());
	return;
    }


    /* username */
    /* If there's a Local-Mailbox profile component, try to extract
       the username from it.  But don't try very hard, this assumes
       the very simple User Name <user@name.com> form.
       Note that post(8) and whom(1) use context_foil (), so they
       won't see the profile component. */
    if ((np = context_find("Local-Mailbox")) != NULL) {
	char *left_angle_bracket = strchr (np, '<');
	char *at_sign = strchr (np, '@');
	char *right_angle_bracket = strchr (np, '>');

	strncpy(localmbox, np, sizeof(localmbox));

	if (left_angle_bracket	&&  at_sign  &&	 right_angle_bracket) {
	    if (at_sign > left_angle_bracket  &&
		at_sign - left_angle_bracket < BUFSIZ) {
		strncpy(username, left_angle_bracket + 1,
			at_sign - left_angle_bracket - 1);
	    }
	}
    }

    if (username[0] == '\0') {
	strncpy (username, pw->pw_name, sizeof(username));
    }

    username[sizeof(username) - 1] = '\0';

    escape_local_part(username, sizeof(username));


    /* fullname */
    np = pw->pw_gecos;

    /* Get the user's real name from the GECOS field.  Stop once we hit a ',',
       which some OSes use to separate other 'finger' information in the GECOS
       field, like phone number. */
    for (cp = fullname; *np != '\0' && *np != ','; *cp++ = *np++)
	continue;
    *cp = '\0';

    /* The $SIGNATURE environment variable overrides the GECOS field's idea of
       your real name. If SIGNATURE isn't set, use the Signature profile
       setting if it exists. */
    if ((cp = getenv ("SIGNATURE")) && *cp)
	strncpy (fullname, cp, sizeof(fullname));
    else if ((cp = context_find("Signature")))
    	strncpy (fullname, cp, sizeof(fullname));

    fullname[sizeof(fullname) - 1] = '\0';

    escape_display_name(fullname, sizeof(fullname));


    /* localmbox, if not using Local-Mailbox */
    if (localmbox[0] == '\0') {
	snprintf(localmbox, sizeof(localmbox), "%s <%s@%s>", fullname,
		 username, LocalName(0));
    }

    localmbox[sizeof(localmbox) - 1] = '\0';
}

static const char*
get_mtsconf_pathname (void)
{
    const char *cp = getenv ( "MHMTSCONF" );
    if (cp != NULL && *cp != '\0') {
        return cp;
    }
    return mtsconf;
}

static const char*
get_mtsuserconf_pathname (void)
{
    const char *cp = getenv ( "MHMTSUSERCONF" );
    if (cp != NULL && *cp != '\0') {
        return cp;
    }
    return NULL;
}

static void
mts_read_conf_file (FILE *fp)
{
    unsigned char *bp;
    char *cp, buffer[BUFSIZ];
    struct bind *b;

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
}
