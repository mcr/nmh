/*
 * seq_list.c -- Get all messages in a sequence and return them
 *            -- as a space separated list of message ranges.
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>

/* allocate this much buffer space at a time */
#define MAXBUFFER 1024

/* static buffer to collect the sequence line */
static char *buffer = NULL;
static int len = 0;


char *
seq_list(struct msgs *mp, char *seqname)
{
    int i, j, seqnum;
    char *bp;

    /* On first invocation, allocate initial buffer space */
    if (!buffer) {
	len = MAXBUFFER;
	buffer = mh_xmalloc ((size_t) len);
    }

    /*
     * Special processing for "cur" sequence.  We assume that the
     * "cur" sequence and mp->curmsg are in sync (see seq_add.c).
     * This is returned, even if message doesn't exist or the
     * folder is empty.
     */
    if (!strcmp (current, seqname)) {
	if (mp->curmsg) {	
	    snprintf(buffer, len, "%s", m_name(mp->curmsg));
	    return (buffer);
	}
        return (NULL);
    }

    /* If the folder is empty, just return NULL */
    if (mp->nummsg == 0)
	return NULL;

    /* Get the index of the sequence */
    if ((seqnum = seq_getnum (mp, seqname)) == -1)
	return NULL;

    bp = buffer;

    for (i = mp->lowmsg; i <= mp->hghmsg; ++i) {
	/*
	 * If message doesn't exist, or isn't in
	 * the sequence, then continue.
	 */
	if (!does_exist(mp, i) || !in_sequence(mp, seqnum, i))
	    continue;

	/*
	 * See if we need to enlarge buffer.  Since we don't know
	 * exactly how many character this particular message range
	 * will need, we enlarge the buffer if we are within
	 * 50 characters of the end.
	 */
	if (bp - buffer > len - 50) {
	    char *newbuf;

	    len += MAXBUFFER;
	    newbuf = mh_xrealloc (buffer, (size_t) len);
	    bp = newbuf + (bp - buffer);
	    buffer = newbuf;
	}

	/*
	 * If this is not the first message range in
	 * the list, first add a space.
	 */
	if (bp > buffer)
	    *bp++ = ' ';

	strcpy(bp, m_name(i));
	bp += strlen(bp);
	j = i;			/* Remember beginning of message range */

	/*
	 * Scan to the end of this message range
	 */
	for (++i; i <= mp->hghmsg && does_exist(mp, i) && in_sequence(mp, seqnum, i);
	     ++i)
	    ;

	if (i - j > 1) {
            *bp++ = '-';
	    strcpy(bp, m_name(i - 1));
	    bp += strlen(bp);
	}
    }
    return (bp > buffer? buffer : NULL);
}
