
/*
 * m_convert.c -- parse a message range or sequence and set SELECTED
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

/*
 * error codes for sequence
 * and message range processing
 */
#define	BADMSG	(-2)
#define	BADRNG	(-3)
#define	BADNEW	(-4)
#define	BADNUM	(-5)
#define	BADLST	(-6)

#define	FIRST	1
#define	LAST	2

#define	getnew(mp) (mp->hghmsg + 1)

static int convdir;	/* convert direction */
static char *delimp;

/*
 * static prototypes
 */
static int m_conv (struct msgs *, char *, int);
static int attr (struct msgs *, char *);


int
m_convert (struct msgs *mp, char *name)
{
    int first, last, found, range, err;
    unsigned char *bp;
    char *cp;

    /* check if user defined sequence */
    err = attr (mp, cp = name);

    if (err == -1)
	return 0;
    else if (err < 0)
	goto badmsg;
    else if (err > 0)
	return 1;
    /*
     * else err == 0, so continue
     */

    found = 0;

    /*
     * Check for special "new" sequence, which
     * is valid only if ALLOW_NEW is set.
     */
    if ((mp->msgflags & ALLOW_NEW) && !strcmp (cp, "new")) {
	if ((err = first = getnew (mp)) <= 0)
	    goto badmsg;
	else
	    goto single;
    }

    if (!strcmp (cp, "all"))
	cp = "first-last";

    if ((err = first = m_conv (mp, cp, FIRST)) <= 0)
	goto badmsg;

    cp = delimp;
    if (*cp != '\0' && *cp != '-' && *cp != ':') {
badelim:
	advise (NULL, "illegal argument delimiter: `%c'(0%o)", *delimp, *delimp);
	return 0;
    }

    if (*cp == '-') {
	cp++;
	if ((err = last = m_conv (mp, cp, LAST)) <= 0) {
badmsg:
	    switch (err) {
	    case BADMSG: 
		advise (NULL, "no %s message", cp);
		break;

	    case BADNUM: 
		advise (NULL, "message %s doesn't exist", cp);
		break;

	    case BADRNG: 
		advise (NULL, "message %s out of range 1-%d", cp, mp->hghmsg);
		break;

	    case BADLST: 
badlist:
		advise (NULL, "bad message list %s", name);
		break;

	    case BADNEW:
		advise (NULL, "folder full, no %s message", name);
		break;

	    default: 
		advise (NULL, "no messages match specification");
	    }
	    return 0;
	}

	if (last < first)
	    goto badlist;
	if (*delimp)
	    goto badelim;
	if (first > mp->hghmsg || last < mp->lowmsg) {
rangerr:
	    advise (NULL, "no messages in range %s", name);
	    return 0;
	}

	/* tighten the range to search */
	if (last > mp->hghmsg)
	    last = mp->hghmsg;
	if (first < mp->lowmsg)
	    first = mp->lowmsg;

    } else if (*cp == ':') {
	cp++;
	if (*cp == '-') {
	    convdir = -1;
	    cp++;
	} else {
	    if (*cp == '+') {
		convdir = 1;
		cp++;
	    }
	}
	if ((range = atoi (bp = cp)) == 0)
	    goto badlist;
	while (isdigit (*bp))
	    bp++;
	if (*bp)
	    goto badelim;
	if ((convdir > 0 && first > mp->hghmsg)
	    || (convdir < 0 && first < mp->lowmsg))
	    goto rangerr;

	/* tighten the range to search */
	if (first < mp->lowmsg)
	    first = mp->lowmsg;
	if (first > mp->hghmsg)
	    first = mp->hghmsg;

	for (last = first;
	     last >= mp->lowmsg && last <= mp->hghmsg;
	     last += convdir)
	    if (does_exist (mp, last))
		if (--range <= 0)
		    break;
	if (last < mp->lowmsg)
	    last = mp->lowmsg;
	if (last > mp->hghmsg)
	    last = mp->hghmsg;
	if (last < first) {
	    range = last;
	    last = first;
	    first = range;
	}
    } else {

single:
	/*
	 * Single Message
	 *
	 * If ALLOW_NEW is set, then allow selecting of an
	 * empty slot.  If ALLOW_NEW is not set, then we
	 * check if message is in-range and exists.
	 */
	if (mp->msgflags & ALLOW_NEW) {
	    set_select_empty (mp, first);
	} else {
	    if (first > mp->hghmsg
		|| first < mp->lowmsg
		|| !(does_exist (mp, first))) {
		if (!strcmp (name, "cur") || !strcmp (name, "."))
		    advise (NULL, "no %s message", name);
		else
		    advise (NULL, "message %d doesn't exist", first);
		return 0;
	    }
	}
	last = first;	/* range of 1 */
    }

    /*
     * Cycle through the range and select the messages
     * that exist.  If ALLOW_NEW is set, then we also check
     * if we are selecting an empty slot.
     */
    for (; first <= last; first++) {
	if (does_exist (mp, first) ||
	    ((mp->msgflags & ALLOW_NEW) && is_select_empty (mp, first))) {
	    if (!is_selected (mp, first)) {
		set_selected (mp, first);
		mp->numsel++;
		if (mp->lowsel == 0 || first < mp->lowsel)
		    mp->lowsel = first;
		if (first > mp->hghsel)
		    mp->hghsel = first;
	    }
	    found++;
	}
    }

    if (!found)
	goto rangerr;

    return 1;
}

/*
 * Convert the various message names to
 * their numeric values.
 *
 * n     (integer)
 * prev
 * next
 * first
 * last
 * cur
 * .     (same as cur)
 */

static int
m_conv (struct msgs *mp, char *str, int call)
{
    register int i;
    register unsigned char *cp, *bp;
    unsigned char buf[16];

    convdir = 1;
    cp = bp = str;
    if (isdigit (*cp)) {
	while (isdigit (*bp))
	    bp++;
	delimp = bp;
	i = atoi (cp);

	if (i <= mp->hghmsg)
	    return i;
	else if (*delimp || call == LAST)
	    return mp->hghmsg + 1;
	else if (mp->msgflags & ALLOW_NEW)
	    return BADRNG;
	else
	    return BADNUM;
    }

#ifdef LOCALE
    /* doesn't enforce lower case */
    for (bp = buf; (isalpha(*cp) || *cp == '.')
		&& (bp - buf < sizeof(buf) - 1); )
#else
    for (bp = buf; ((*cp >= 'a' && *cp <= 'z') || *cp == '.')
		&& (bp - buf < sizeof(buf) - 1); )
#endif /* LOCALE */
    {
	*bp++ = *cp++;
    }
    *bp++ = '\0';
    delimp = cp;

    if (!strcmp (buf, "first"))
	return (mp->hghmsg || !(mp->msgflags & ALLOW_NEW)
		? mp->lowmsg : BADMSG);

    if (!strcmp (buf, "last")) {
	convdir = -1;
	return (mp->hghmsg || !(mp->msgflags & ALLOW_NEW) ? mp->hghmsg : BADMSG);
    }

    if (!strcmp (buf, "cur") || !strcmp (buf, "."))
	return (mp->curmsg > 0 ? mp->curmsg : BADMSG);

    if (!strcmp (buf, "prev")) {
	convdir = -1;
	for (i = (mp->curmsg <= mp->hghmsg) ? mp->curmsg - 1 : mp->hghmsg;
		i >= mp->lowmsg; i--) {
	    if (does_exist (mp, i))
		return i;
	}
	return BADMSG;
    }

    if (!strcmp (buf, "next")) {
	for (i = (mp->curmsg >= mp->lowmsg) ? mp->curmsg + 1 : mp->lowmsg;
		i <= mp->hghmsg; i++) {
	    if (does_exist (mp, i))
		return i;
	}
	return BADMSG;
    }

    return BADLST;
}

/*
 * Handle user defined sequences.
 * They can take the following forms:
 *
 * seq
 * seq:prev
 * seq:next
 * seq:first
 * seq:last
 * seq:+n
 * seq:-n
 * seq:n
 */

static int
attr (struct msgs *mp, char *cp)
{
    register unsigned char *dp;
    char *bp = NULL;
    register int i, j;
    int found,
	inverted = 0,
	range = 0,		/* no range */
	first = 0;

    /* hack for "cur-name", "cur-n", etc. */
    if (!strcmp (cp, "cur"))
	return 0;
    if (ssequal ("cur:", cp))	/* this code need to be rewritten... */
	return 0;

    /* Check for sequence negation */
    if ((dp = context_find (nsequence)) && *dp != '\0' && ssequal (dp, cp)) {
	inverted = 1;
	cp += strlen (dp);
    }

    convdir = 1;	/* convert direction */

    for (dp = cp; *dp && isalnum(*dp); dp++)
	continue;

    if (*dp == ':') {
	bp = dp++;
	range = 1;

	/*
	 * seq:prev  (or)
	 * seq:next  (or)
	 * seq:first (or)
	 * seq:last
	 */
	if (isalpha (*dp)) {
	    if (!strcmp (dp, "prev")) {
		convdir = -1;
		first = (mp->curmsg > 0) && (mp->curmsg <= mp->hghmsg)
			? mp->curmsg - 1
			: mp->hghmsg;
	    }
	    else if (!strcmp (dp, "next")) {
		convdir = 1;
		first = (mp->curmsg >= mp->lowmsg)
			    ? mp->curmsg + 1
			    : mp->lowmsg;
	    }
	    else if (!strcmp (dp, "first")) {
		convdir = 1;
	    }
	    else if (!strcmp (dp, "last")) {
		convdir = -1;
	    }
	    else
		return BADLST;
	} else {
	    /*
	     * seq:n  (or)
	     * seq:+n (or)
	     * seq:-n
             */
	    if (*dp == '+')
		dp++;
	    else if (*dp == '-') {
		dp++;
		convdir = -1;
	    }
	    if ((range = atoi(dp)) == 0)
		return BADLST;
	    while (isdigit (*dp))
		dp++;
	    if (*dp)
		return BADLST;
	}

	*bp = '\0';	/* temporarily terminate sequence name */
    }

    i = seq_getnum (mp, cp);	/* get index of sequence */

    if (bp)
	*bp = ':';		/* restore sequence name */
    if (i == -1)
	return 0;

    found = 0;	/* count the number we select for this argument */

    for (j = first ? first : (convdir > 0) ? mp->lowmsg : mp->hghmsg;
		j >= mp->lowmsg && j <= mp->hghmsg; j += convdir) {
	if (does_exist (mp, j)
		&& inverted ? !in_sequence (mp, i, j) : in_sequence (mp, i, j)) {
	    if (!is_selected (mp, j)) {
		set_selected (mp, j);
		mp->numsel++;
		if (mp->lowsel == 0 || j < mp->lowsel)
		    mp->lowsel = j;
		if (j > mp->hghsel)
		    mp->hghsel = j;
	    }
	    found++;

	    /*
	     * If we have a range, then break out
	     * once we've found enough.
             */
	    if (range && found >= range)
		break;
	}
    }

    if (found > 0)
	return found;

    if (first)
	return BADMSG;
    advise (NULL, "sequence %s %s", cp, inverted ? "full" : "empty");
    return -1;
}
