/* m_convert.c -- parse a message range or sequence and set SELECTED
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>

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
    int first, last, found, count, is_range, err;
    char *bp, *cp;

    /* check if user defined sequence */
    err = attr (mp, cp = name);

    if (err == -1)
	return 0;
    if (err < 0)
	goto badmsg;
    if (err > 0)
	return 1;
    /*
     * else err == 0, so continue
     */

    found = 0;
    is_range = 1;

    /*
     * Check for special "new" sequence, which
     * is valid only if ALLOW_NEW is set.
     */
    if ((mp->msgflags & ALLOW_NEW) && !strcmp (cp, "new")) {
	if ((err = first = getnew (mp)) <= 0)
	    goto badmsg;
        goto single;
    }

    if (!strcmp (cp, "all"))
	cp = "first-last";

    if ((err = first = m_conv (mp, cp, FIRST)) <= 0)
	goto badmsg;

    cp = delimp;
    if (*cp != '\0' && *cp != '-' && *cp != ':' && *cp != '=') {
badelim:
	inform("illegal argument delimiter: `%c'(0%o)", *delimp, *delimp);
	return 0;
    }

    if (*cp == '-') {
	cp++;
	if ((err = last = m_conv (mp, cp, LAST)) <= 0) {
badmsg:
	    switch (err) {
	    case BADMSG: 
		inform("no %s message", cp);
		break;

	    case BADNUM: 
		inform("message %s doesn't exist", cp);
		break;

	    case BADRNG: 
		inform("message %s out of range 1-%d", cp, mp->hghmsg);
		break;

	    case BADLST: 
badlist:
		inform("bad message list %s", name);
		break;

	    case BADNEW:
		inform("folder full, no %s message", name);
		break;

	    default: 
		inform("no messages match specification");
	    }
	    return 0;
	}

	if (last < first)
	    goto badlist;
	if (*delimp)
	    goto badelim;
	if (first > mp->hghmsg || last < mp->lowmsg) {
rangerr:
	    inform("no messages in range %s", name);
	    return 0;
	}

	/* tighten the range to search */
	if (last > mp->hghmsg)
	    last = mp->hghmsg;
	if (first < mp->lowmsg)
	    first = mp->lowmsg;

    } else if (*cp == ':' || *cp == '=') {

	if (*cp == '=')
	    is_range = 0;

	cp++;

	if (*cp == '-') {
	    /* foo:-3 or foo=-3 */
	    convdir = -1;
	    cp++;
	} else if (*cp == '+') {
	    /* foo:+3 or foo=+3 is same as foo:3 or foo=3 */
	    convdir = 1;
	    cp++;
	}
	if ((count = atoi (bp = cp)) == 0)
	    goto badlist;
	while (isdigit ((unsigned char) *bp))
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
		if (--count <= 0)
		    break;
	if (is_range) { /* a range includes any messages that exist */
	    if (last < mp->lowmsg)
		last = mp->lowmsg;
	    if (last > mp->hghmsg)
		last = mp->hghmsg;
	    if (last < first) {
		count = last;
		last = first;
		first = count;
	    }
	} else { /* looking for the nth message.  if not enough, fail. */
	    if (last < mp->lowmsg || last > mp->hghmsg) {
		inform("no such message");
		return 0;
	    }
	    first = last;
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
	    /*
	     * We can get into a case where the "cur" sequence is way out
	     * of range, and because it's allowed to not exist (think
	     * of "rmm; next") it doesn't get checked to make sure it's
	     * within the range of messages in seq_init().  So if our
	     * desired sequence is out of range of the allocated folder
	     * limits simply reallocate the folder so it's within range.
	     */
	    if (first < mp->lowoff || first > mp->hghoff)
                mp = folder_realloc(mp, min(first, mp->lowoff),
                                    max(first, mp->hghoff));

	    set_select_empty (mp, first);
	} else {
	    if (first > mp->hghmsg
		|| first < mp->lowmsg
		|| !(does_exist (mp, first))) {
		if (!strcmp (name, "cur") || !strcmp (name, "."))
		    inform("no %s message", name);
		else
		    inform("message %d doesn't exist", first);
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
    int i;
    char *cp, *bp;
    char buf[16];

    convdir = 1;
    cp = bp = str;
    if (isdigit ((unsigned char) *cp)) {
	while (isdigit ((unsigned char) *bp))
	    bp++;
	delimp = bp;
	i = atoi (cp);

	if (i <= mp->hghmsg)
	    return i;
	if (*delimp || call == LAST)
	    return mp->hghmsg + 1;
	if (mp->msgflags & ALLOW_NEW)
	    return BADRNG;
        return BADNUM;
    }

    /* doesn't enforce lower case */
    for (bp = buf; (isalpha((unsigned char) *cp) || *cp == '.')
           && (bp - buf < (int) sizeof(buf) - 1); )
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
    char *dp;
    char *bp = NULL;
    char *ep;
    char op;
    int i, j;
    int found,
	inverted = 0,
	count = 0,		/* range given?  else use entire sequence */
	just_one = 0,		/* want entire sequence or range */
	first = 0,
	start = 0;

    /* hack for "cur-name", "cur-n", etc. */
    if (!strcmp (cp, "cur"))
	return 0;
    if (has_prefix(cp, "cur")) {
	if (cp[3] == ':' || cp[3] == '=')
	    return 0;
    }

    /* Check for sequence negation */
    if ((dp = context_find (nsequence)) && *dp != '\0' && ssequal (dp, cp)) {
	inverted = 1;
	cp += strlen (dp);
    }

    convdir = 1;	/* convert direction */

    for (dp = cp; *dp && isalnum((unsigned char) *dp); dp++)
	continue;

    if (*dp == ':') {
	bp = dp++;
	count = 1;

	/*
	 * seq:prev  (or)
	 * seq:next  (or)
	 * seq:first (or)
	 * seq:last
	 */
	if (isalpha ((unsigned char) *dp)) {
	    if (!strcmp (dp, "prev")) {
		convdir = -1;
		first = (mp->curmsg > 0) && (mp->curmsg <= mp->hghmsg)
			? mp->curmsg - 1
			: mp->hghmsg;
		start = first;
	    }
	    else if (!strcmp (dp, "next")) {
		convdir = 1;
		first = (mp->curmsg >= mp->lowmsg)
			    ? mp->curmsg + 1
			    : mp->lowmsg;
		start = first;
	    }
	    else if (!strcmp (dp, "first")) {
		convdir = 1;
		start = mp->lowmsg;
	    }
	    else if (!strcmp (dp, "last")) {
		convdir = -1;
		start = mp->hghmsg;
	    }
	    else {
		return BADLST;
	    }
	} else {
	    /*
	     * seq:n  (or)
	     * seq:+n (or)
	     * seq:-n
             */
	    if (*dp == '+') {
		/* foo:+3 is same as foo:3 */
		dp++;
		convdir = 1;
		start = mp->lowmsg;
	    } else if (*dp == '-' || *dp == ':') {
		/* foo:-3 or foo::3 */
		dp++;
		convdir = -1;
		start = mp->hghmsg;
	    }
	    count = strtol(dp,&ep,10);
	    if (count == 0 || *ep)     /* 0 illegal */
		return BADLST;
	}

	op = *bp;
	*bp = '\0';	/* temporarily terminate sequence name */
    } else if (*dp == '=') {

	bp = dp++;

	if (*dp == '+') {
	    /* foo=+3 is same as foo=3 */
	    dp++;
	    convdir = 1;
	    start = mp->lowmsg;
	} else if (*dp == '-') {
	    /* foo=-3 */
	    dp++;
	    convdir = -1;
	    start = mp->hghmsg;
	}

	count = strtol(dp,&ep,10);     /* 0 illegal */
	if (count == 0 || *ep)
	    return BADLST;

	just_one = 1;

	op = *bp;
	*bp = '\0';	/* temporarily terminate sequence name */
    }

    i = seq_getnum (mp, cp);	/* get index of sequence */

    if (bp)
	*bp = op;		/* restore sequence name */
    if (i == -1)
	return 0;

    found = 0;	/* count the number we select for this argument */

    if (!start)
	start = mp->lowmsg;

    for (j = start; j >= mp->lowmsg && j <= mp->hghmsg; j += convdir) {

	if (does_exist (mp, j)
		&& inverted ? !in_sequence (mp, i, j) : in_sequence (mp, i, j)) {
	    found++;
	    /* we want all that we find, or just the last in the +/_ case */
	    if (!just_one || found >= count) {
		if (!is_selected (mp, j)) {
		    set_selected (mp, j);
		    mp->numsel++;
		    if (mp->lowsel == 0 || j < mp->lowsel)
			mp->lowsel = j;
		    if (j > mp->hghsel)
			mp->hghsel = j;
		}
	    }
	    /*
	     * If we have any sort of limit, then break out
	     * once we've found enough.
             */
	    if (count && found >= count)
		break;

	}
    }

    if (mp->numsel > 0)
	return mp->numsel;

    if (first || just_one)
	return BADMSG;
    inform("sequence %s %s", cp, inverted ? "full" : "empty");
    return -1;
}
