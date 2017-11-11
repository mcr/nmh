/* addrsbr.c -- parse addresses 822-style
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "uprf.h"
#include "getcpy.h"
#include "context_find.h"
#include "error.h"
#include "h/addrsbr.h"
#include "h/mf.h"
#include "h/mts.h"
#include "h/utils.h"

/* High level parsing of addresses:

   The routines in sbr/mf.c parse the syntactic representations of
   addresses.  The routines in sbr/addrsbr.c associate semantics with those
   addresses.  

   The comments below are left in for historical purposes; DUMB and
   REALLYDUMB are now the default in the code.

   If #ifdef DUMB is in effect, a full 822-style parser is called
   for syntax recognition.  This breaks each address into its components.
   Note however that no semantics are assumed about the parts or their
   totality.  This means that implicit hostnames aren't made explicit,
   and explicit hostnames aren't expanded to their "official" representations.

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
   defaulted, typically to the local host.  The lack of an explicit host is
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
static int eai = 0;

void
enable_eai(void)
{
    eai = 1;
}


char *
getname (const char *addrs)
{
    struct adrx *ap;
    pers = mbox = host = route = grp = note = NULL;
    err[0] = '\0';

    if ((ap = getadrx (FENDNULL(addrs), eai)) == NULL) {
	return NULL;
    }

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
getm (char *str, char *dfhost, int dftype, char *eresult, size_t eresultsize)
{
    char *pp;
    struct mailname *mp;

    if (err[0]) {
	if (eresult) {
	    strncpy (eresult, err, eresultsize);
	    eresult[eresultsize - 1] = '\0';
	}
	return NULL;
    }
    if (pers == NULL
	    && mbox == NULL && host == NULL && route == NULL
	    && grp == NULL) {
	if (eresult) {
	    strncpy (eresult, "null address", eresultsize);
	    eresult[eresultsize - 1] = '\0';
	}
	return NULL;
    }
    if (mbox == NULL && grp == NULL) {
	if (eresult) {
	    strncpy (eresult, "no mailbox in address", eresultsize);
	    eresult[eresultsize - 1] = '\0';
	}
	return NULL;
    }

    if (dfhost == NULL) {
	dfhost = LocalName (0);
	dftype = LOCALHOST;
    }

    NEW0(mp);
    mp->m_next = NULL;
    mp->m_text = getcpy (str);
    if (pers)
	mp->m_pers = mh_xstrdup(pers);

    if (mbox == NULL) {
	mp->m_type = BADHOST;
	mp->m_nohost = 1;
	mp->m_ingrp = ingrp;
	mp->m_gname = getcpy (grp);
	if (note)
	    mp->m_note = mh_xstrdup(note);
	return mp;
    }

    if (host) {
	mp->m_mbox = getcpy (mbox);
	mp->m_host = mh_xstrdup(host);
	mp->m_type =
	    strcasecmp (LocalName(0), mp->m_host) ? NETHOST : LOCALHOST;
    } else {
	if ((pp = strchr(mbox, '!'))) {
	    *pp++ = '\0';
	    mp->m_mbox = mh_xstrdup(pp);
	    mp->m_host = getcpy (mbox);
	    mp->m_type = UUCPHOST;
	} else {
	    mp->m_nohost = 1;
	    mp->m_mbox = getcpy (mbox);
	    if (route == NULL && dftype == LOCALHOST) {
		mp->m_host = NULL;
		mp->m_type = dftype;
	    } else {
		mp->m_host = route ? NULL : getcpy (dfhost);
		mp->m_type = route ? NETHOST : dftype;
	    }
	}
    }

    /* For alternate mailboxes, m_type gets overwritten in ismymbox ()
       to support wild-card matching. */

    if (route)
	mp->m_path = mh_xstrdup(route);
    mp->m_ingrp = ingrp;
    if (grp)
	mp->m_gname = mh_xstrdup(grp);
    if (note)
	mp->m_note = mh_xstrdup(note);

    return mp;
}


void
mnfree (struct mailname *mp)
{
    if (!mp)
	return;

    free(mp->m_text);
    free(mp->m_pers);
    free(mp->m_mbox);
    free(mp->m_host);
    free(mp->m_path);
    free(mp->m_gname);
    free(mp->m_note);

    free(mp);
}


char *
auxformat (struct mailname *mp, int extras)
{
    static char addr[BUFSIZ];
    static char buffer[BUFSIZ];

	if (mp->m_nohost)
	    strncpy (addr, FENDNULL(mp->m_mbox), sizeof(addr));
	else if (mp->m_type != UUCPHOST) {
	    if (mp->m_host)
	    	snprintf (addr, sizeof(addr), "%s%s@%s", FENDNULL(mp->m_path),
			  FENDNULL(mp->m_mbox), mp->m_host);
	    else snprintf (addr, sizeof(addr), "%s%s", FENDNULL(mp->m_path),
	    		   FENDNULL(mp->m_mbox));
	} else
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
    } else if (mp->m_note)
        snprintf (buffer, sizeof(buffer), "%s %s", addr, mp->m_note);
    else
        strncpy (buffer, addr, sizeof(buffer));

    return buffer;
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

bool
ismymbox (struct mailname *np)
{
    bool oops;
    int len, i;
    char *cp;
    char *pp;
    char buffer[BUFSIZ];
    struct mailname *mp;
    static char *am = NULL;
    static struct mailname mq;
    static bool localmailbox;

    /*
     * If this is the first call, initialize
     * list of alternate mailboxes.
     */
    if (am == NULL) {
	mq.m_next = NULL;
	mq.m_mbox = getusername ();

	if ((am = context_find ("local-mailbox"))) {

	    localmailbox = true;

	    if ((cp = getname(am)) == NULL) {
	        inform("Unable to find address in local-mailbox, continuing...");
		return false;
	    }

	    if ((mq.m_next = getm (cp, NULL, 0, NULL, 0)) == NULL) {
                inform("invalid entry in local-mailbox: %s, continuing...", cp);
		return false;
	    }

	    /* Sigh, it turns out that the address parser gets messed up
	     * if you don't call getname() until it returns NULL. */
	    while (getname(am) != NULL)
	    	;
	}

	if ((am = context_find ("alternate-mailboxes")) == NULL)
	    am = getusername();
	else {
	    mp = mq.m_next ? mq.m_next : &mq;
	    oops = false;
	    while ((cp = getname (am))) {
		if ((mp->m_next = getm (cp, NULL, 0, NULL, 0)) == NULL) {
		    inform("illegal address: %s, continuing...", cp);
		    oops = true;
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
		}
	    }
	    if (oops)
		inform("please fix the %s: entry in your %s file",
			"alternate-mailboxes", mh_profile);
	}

	if ((cp = getenv ("MHWDEBUG")) && *cp) {
	    for (mp = &mq; mp; mp = mp->m_next) {
	      fprintf (stderr, "Local- or Alternate-Mailbox: text=\"%s\" "
		       "mbox=\"%s\" host=\"%s\" %s\n",
		       FENDNULL(mp->m_text), mp->m_mbox,
		       FENDNULL(mp->m_host),
		       snprintb (buffer, sizeof(buffer), (unsigned) mp->m_type,
				 WBITS));
	    }
	}
    }

    if (np == NULL) /* XXX */
	return false;
    
    /*
     * Don't perform this "local" test if we have a Local-Mailbox set
     */

    if (! localmailbox)
	switch (np->m_type) {
	    case NETHOST:
		len = strlen (cp = LocalName (0));
		if (!uprf (np->m_host, cp) || np->m_host[len] != '.')
		    break;
		goto local_test;

	    case UUCPHOST:
		if (strcasecmp (np->m_host, SystemName()))
		    break;
		    /* FALLTHRU */
	    case LOCALHOST:
local_test: ;
		if (!strcasecmp (np->m_mbox, mq.m_mbox))
		    return true;
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
	    return true;
	if (np->m_host == NULL || mp->m_host == NULL)
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
	return true;
    }

    return false;
}
