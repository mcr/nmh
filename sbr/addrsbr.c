
/*
 * addrsbr.c -- parse addresses 822-style
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/addrsbr.h>
#include <h/mf.h>

/* High level parsing of addresses:

   The routines in sbr/mf.c parse the syntactic representations of
   addresses.  The routines in sbr/addrsbr.c associate semantics with those
   addresses.  

   If #ifdef DUMB is in effect, a full 822-style parser is called
   for syntax recongition.  This breaks each address into its components.
   Note however that no semantics are assumed about the parts or their
   totality.  This means that implicit hostnames aren't made explicit,
   and explicit hostnames aren't expanded to their "official" represenations.

   If DUMB is not in effect, then this module does some
   high-level thinking about what the addresses are.

   1. for MMDF systems:

	string%<uucp>@<local>	->	string

   2. for non-MMDF systems:

	string@host.<uucp>	->	host!string

   3. for any system, an address interpreted relative to the local host:

	string@<uucp>		->	string

   For cases (1) and (3) above, the leftmost host is extracted.  If it's not
   present, the local host is used.  If the tests above fail, the address is
   considered to be a real 822-style address.

   If an explicit host is not present, then MH checks for a bang to indicate
   an explicit UUCP-style address.  If so, this is noted.  If not, the host is
   defaulted, typically to the local host.  The lack of an explict host is
   also noted.

   If an explicit 822-style host is present, then MH checks to see if it
   can expand this to the official name for the host.  If the hostname is
   unknown, the address is so typed.

   To summarize, when we're all done, here's what MH knows about the address:

   DUMB	-	type:	local, uucp, or network
		host:	not locally defaulted, not explicitly expanded
		everything else

   other -	type:	local, uucp, network, unknown
		everything else
 */


static int  ingrp = 0;
static char *pers = NULL;
static char *mbox = NULL;
static char *host = NULL;
static char *route = NULL;
static char *grp = NULL;
static char *note = NULL;
static char err[BUFSIZ];
static char adr[BUFSIZ];


extern boolean  username_extension_masquerading;  /* defined in mts.c */


/*
 * external prototypes
 */
char *getusername (void);


char *
getname (char *addrs)
{
    struct adrx *ap;

    pers = mbox = host = route = grp = note = NULL;
    err[0] = '\0';

    if ((ap = getadrx (addrs ? addrs : "")) == NULL)
	return NULL;

    strncpy (adr, ap->text, sizeof(adr));
    pers = ap->pers;
    mbox = ap->mbox;
    host = ap->host;
    route = ap->path;
    grp = ap->grp;
    ingrp = ap->ingrp;
    note = ap->note;
    if (ap->err && *ap->err)
	strncpy (err, ap->err, sizeof(err));

    return adr;
}


struct mailname *
getm (char *str, char *dfhost, int dftype, int wanthost, char *eresult)
{
    char *pp;
    struct mailname *mp;
#ifndef	DUMB
    char *dp;
#endif /* not DUMB */

    if (err && err[0]) {
	if (eresult)
	    strcpy (eresult, err);
	else
	    if (wanthost == AD_HOST)
		admonish (NULL, "bad address '%s' - %s", str, err);
	return NULL;
    }
    if (pers == NULL
	    && mbox == NULL && host == NULL && route == NULL
	    && grp == NULL) {
	if (eresult)
	    strcpy (eresult, "null address");
	else
	    if (wanthost == AD_HOST)
		admonish (NULL, "null address '%s'", str);
	return NULL;
    }
    if (mbox == NULL && grp == NULL) {
	if (eresult)
	    strcpy (eresult, "no mailbox in address");
	else
	    if (wanthost == AD_HOST)
		admonish (NULL, "no mailbox in address '%s'", str);
	return NULL;
    }

    if (dfhost == NULL) {
	dfhost = LocalName ();
	dftype = LOCALHOST;
    }

    mp = (struct mailname *) calloc ((size_t) 1, sizeof(*mp));
    if (mp == NULL) {
	if (eresult)
	   strcpy (eresult, "insufficient memory to represent address");
	else
	    if (wanthost == AD_HOST)
		adios (NULL, "insufficient memory to represent address");
	return NULL;
    }

    mp->m_next = NULL;
    mp->m_text = getcpy (str);
    if (pers)
	mp->m_pers = getcpy (pers);

    if (mbox == NULL) {
	mp->m_type = BADHOST;
	mp->m_nohost = 1;
	mp->m_ingrp = ingrp;
	mp->m_gname = getcpy (grp);
	if (note)
	    mp->m_note = getcpy (note);
	return mp;
    }

    if (host) {
	mp->m_mbox = getcpy (mbox);
	mp->m_host = getcpy (host);
    }
    else {
	if ((pp = strchr(mbox, '!'))) {
	    *pp++ = '\0';
	    mp->m_mbox = getcpy (pp);
	    mp->m_host = getcpy (mbox);
	    mp->m_type = UUCPHOST;
	}
	else {
	    mp->m_nohost = 1;
	    mp->m_mbox = getcpy (mbox);
#ifdef DUMB
	    if (route == NULL && dftype == LOCALHOST) {
		mp->m_host = NULL;
		mp->m_type = dftype;
	    }
	    else
#endif /* DUMB */
	    {
		mp->m_host = route ? NULL : getcpy (dfhost);
		mp->m_type = route ? NETHOST : dftype;
	    }
	}
	goto got_host;
    }

    if (wanthost == AD_NHST)
	mp->m_type = !strcasecmp (LocalName (), mp->m_host)
	    ? LOCALHOST : NETHOST;
#ifdef DUMB
    else
	mp->m_type = strcasecmp (LocalName(), mp->m_host) ?  NETHOST : LOCALHOST;
#else /* not DUMB */
    else
	if (pp = OfficialName (mp->m_host)) {
    got_real_host: ;
	    free (mp->m_host);
	    mp->m_host = getcpy (pp);
	    mp->m_type = strcasecmp (LocalName(), mp->m_host) ? NETHOST : LOCALHOST;
	}
	else {
	    if (dp = strchr(mp->m_host, '.')) {
		*dp = NULL;
		if (pp = OfficialName (mp->m_host))
		    goto got_real_host;
		*dp = '.';
	    }
	    mp->m_type = BADHOST;
	}
#endif /* not DUMB */

got_host: ;
    if (route)
	mp->m_path = getcpy (route);
    mp->m_ingrp = ingrp;
    if (grp)
	mp->m_gname = getcpy (grp);
    if (note)
	mp->m_note = getcpy (note);

    return mp;
}


void
mnfree (struct mailname *mp)
{
    if (!mp)
	return;

    if (mp->m_text)
	free (mp->m_text);
    if (mp->m_pers)
	free (mp->m_pers);
    if (mp->m_mbox)
	free (mp->m_mbox);
    if (mp->m_host)
	free (mp->m_host);
    if (mp->m_path)
	free (mp->m_path);
    if (mp->m_gname)
	free (mp->m_gname);
    if (mp->m_note)
	free (mp->m_note);

    free ((char *) mp);
}


#define empty(s) ((s) ? (s) : "")

char *
auxformat (struct mailname *mp, int extras)
{
    static char addr[BUFSIZ];
    static char buffer[BUFSIZ];

#ifdef DUMB
	if (mp->m_nohost)
	    strncpy (addr, mp->m_mbox ? mp->m_mbox : "", sizeof(addr));
	else
#endif /* DUMB */

#ifndef	BANG
	if (mp->m_type != UUCPHOST)
	    snprintf (addr, sizeof(addr), mp->m_host ? "%s%s@%s" : "%s%s",
		empty(mp->m_path), empty(mp->m_mbox), mp->m_host);
	else
#endif /* not BANG */
	    snprintf (addr, sizeof(addr), "%s!%s", mp->m_host, mp->m_mbox);

    if (!extras)
	return addr;

    if (mp->m_pers || mp->m_path) {
	if (mp->m_note)
	    snprintf (buffer, sizeof(buffer), "%s %s <%s>",
		    legal_person (mp->m_pers ? mp->m_pers : mp->m_mbox),
		    mp->m_note, addr);
	else
	    snprintf (buffer, sizeof(buffer), "%s <%s>",
		    legal_person (mp->m_pers ? mp->m_pers : mp->m_mbox),
		    addr);
    }
    else
	if (mp->m_note)
	    snprintf (buffer, sizeof(buffer), "%s %s", addr, mp->m_note);
	else
	    strncpy (buffer, addr, sizeof(buffer));

    return buffer;
}


/*
 * address specific "sprintf"
 */

char *
adrsprintf (char *username, char *domain)
{
    int          snprintf_return;
    static char  addr[BUFSIZ];

    if (username == NULL)
	username = getusername();

    if (username_extension_masquerading) {
	/* mts.conf contains "masquerade:[...]username_extension[...]", so tack
	   on the value of the $USERNAME_EXTENSION environment variable, if set,
	   to username. */
	char*        extension = getenv("USERNAME_EXTENSION");
	static char  username_with_extension[BUFSIZ];

	if (extension != NULL && *extension != '\0') {
	    snprintf_return = snprintf(username_with_extension,
				       sizeof(username_with_extension),
				       "%s%s", username, extension);
	    
	    if (snprintf_return < 0 ||
		snprintf_return >= sizeof(username_with_extension))
		adios(NULL, "snprintf() error writing username (%d chars) and"
		      " $USERNAME_EXTENSION (%d chars) to array of BUFSIZ (%d)"
		      " chars",
		      strlen(username), strlen(extension), BUFSIZ);
	    
	    username = username_with_extension;
	}
    }

#ifdef REALLYDUMB
    return username;
#endif

    if (domain == NULL)
	domain = LocalName();

#ifndef	BANG
    snprintf_return = snprintf (addr, sizeof(addr), "%s@%s", username, domain);
#else /* BANG */
    snprintf_return = snprintf (addr, sizeof(addr), "%s!%s", domain, username);
#endif /* BANG */

    if (snprintf_return < 0 || snprintf_return >= sizeof(addr))
	adios(NULL, "snprintf() error writing username (%d chars), domain (%d"
	      " chars), and 1 separator char to array of BUFSIZ (%d) chars",
	      strlen(username), strlen(domain), BUFSIZ);
    
    return addr;
}


#define	W_NIL	0x0000
#define	W_MBEG	0x0001
#define	W_MEND	0x0002
#define	W_MBOX	(W_MBEG | W_MEND)
#define	W_HBEG	0x0004
#define	W_HEND	0x0008
#define	W_HOST	(W_HBEG | W_HEND)
#define	WBITS	"\020\01MBEG\02MEND\03HBEG\04HEND"

/*
 * Check if this is my address
 */

int
ismymbox (struct mailname *np)
{
    int oops;
    register int len, i;
    register char *cp;
    register char *pp;
    char buffer[BUFSIZ];
    struct mailname *mp;
    static char *am = NULL;
    static struct mailname mq={NULL};

    /*
     * If this is the first call, initialize
     * list of alternate mailboxes.
     */
    if (am == NULL) {
	mq.m_next = NULL;
	mq.m_mbox = getusername ();
	if ((am = context_find ("alternate-mailboxes")) == NULL)
	    am = getusername();
	else {
	    mp = &mq;
	    oops = 0;
	    while ((cp = getname (am))) {
		if ((mp->m_next = getm (cp, NULL, 0, AD_NAME, NULL)) == NULL) {
		    admonish (NULL, "illegal address: %s", cp);
		    oops++;
		} else {
		    mp = mp->m_next;
		    mp->m_type = W_NIL;
		    if (*mp->m_mbox == '*') {
			mp->m_type |= W_MBEG;
			mp->m_mbox++;
		    }
		    if (*(cp = mp->m_mbox + strlen (mp->m_mbox) - 1) == '*') {
			mp->m_type |= W_MEND;
			*cp = '\0';
		    }
		    if (mp->m_host) {
			if (*mp->m_host == '*') {
			    mp->m_type |= W_HBEG;
			    mp->m_host++;
			}
			if (*(cp = mp->m_host + strlen (mp->m_host) - 1) == '*') {
			    mp->m_type |= W_HEND;
			    *cp = '\0';
			}
		    }
		    if ((cp = getenv ("MHWDEBUG")) && *cp)
			fprintf (stderr, "mbox=\"%s\" host=\"%s\" %s\n",
			    mp->m_mbox, mp->m_host,
			    snprintb (buffer, sizeof(buffer), (unsigned) mp->m_type, WBITS));
		}
	    }
	    if (oops)
		advise (NULL, "please fix the %s: entry in your %s file",
			"alternate-mailboxes", mh_profile);
	}
    }

    if (np == NULL) /* XXX */
	return 0;
    
    switch (np->m_type) {
	case NETHOST:
	    len = strlen (cp = LocalName ());
	    if (!uprf (np->m_host, cp) || np->m_host[len] != '.')
		break;
	    goto local_test;

	case UUCPHOST:
	    if (strcasecmp (np->m_host, SystemName()))
		break;		/* fall */
	case LOCALHOST:
local_test: ;
	    if (!strcasecmp (np->m_mbox, mq.m_mbox))
		return 1;
	    break;

	default:
	    break;
    }

    /*
     * Now scan through list of alternate
     * mailboxes, and check for a match.
     */
    for (mp = &mq; mp->m_next;) {
	mp = mp->m_next;
	if (!np->m_mbox)
	    continue;
	if ((len = strlen (cp = np->m_mbox))
		< (i = strlen (pp = mp->m_mbox)))
	    continue;
	switch (mp->m_type & W_MBOX) {
	    case W_NIL: 
		if (strcasecmp (cp, pp))
		    continue;
		break;
	    case W_MBEG: 
		if (strcasecmp (cp + len - i, pp))
		    continue;
		break;
	    case W_MEND: 
		if (!uprf (cp, pp))
		    continue;
		break;
	    case W_MBEG | W_MEND: 
		if (stringdex (pp, cp) < 0)
		    continue;
		break;
	}

	if (mp->m_nohost)
	    return 1;
	if (np->m_host == NULL)
	    continue;
	if ((len = strlen (cp = np->m_host))
		< (i = strlen (pp = mp->m_host)))
	    continue;
	switch (mp->m_type & W_HOST) {
	    case W_NIL: 
		if (strcasecmp (cp, pp))
		    continue;
		break;
	    case W_HBEG: 
		if (strcasecmp (cp + len - i, pp))
		    continue;
		break;
	    case W_HEND: 
		if (!uprf (cp, pp))
		    continue;
		break;
	    case W_HBEG | W_HEND: 
		if (stringdex (pp, cp) < 0)
		    continue;
		break;
	}
	return 1;
    }

    return 0;
}
