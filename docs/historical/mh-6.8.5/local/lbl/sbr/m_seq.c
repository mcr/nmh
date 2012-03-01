/* m_seq.c - print out a message sequence */

#include "../h/mh.h"
#include <stdio.h>

static char *buffer;
static int bufsize = BUFSIZ;

char *
m_seq(mp, cp)
	struct msgs *mp;
	char *cp;
{
	int mask;
	register int i, j, linlen;
	register char *bp, *ep;

	if (buffer == 0) {
		if ((buffer = malloc(bufsize)) == NULL)
			adios (NULLCP, "unable to allocate seq str storage");
	}
	if (strcmp(current, cp) == 0) {
		if (mp->curmsg) {
			(void) sprintf(buffer, "%s", m_name(mp->curmsg));
			return (buffer);
		} else
			return (NULL);
	}
	for (i = 0; mp->msgattrs[i]; i++)
		if (strcmp(mp->msgattrs[i], cp) == 0)
			break;
	
	if (! mp->msgattrs[i])
		return (NULL);

	mask = EXISTS | (1 << (FFATTRSLOT + i));
	bp = buffer;
	ep = bp + bufsize - 16;
	linlen = strlen(cp) + 2;
	for (i = mp->lowmsg; i <= mp->hghmsg; ++i) {
		register char *oldbp;

		if ((mp->msgstats[i] & mask) != mask)
			continue;

		oldbp = bp;
		if (bp > buffer) {
			if (bp >= ep) {
				int oldoff = bp - buffer;
				bufsize <<= 1;
				if ((buffer = realloc(buffer, bufsize)) == NULL)
					adios (NULLCP, "unable to allocate seq str storage");
				oldbp = bp = buffer + oldoff;
				ep = bp + bufsize - 16;
			}
			if (linlen >= 72) {
				linlen = 0;
				*bp++ = '\n';
				*bp++ = ' ';
			}
			*bp++ = ' ';
		}
		(void) sprintf(bp, "%s", m_name(i));
		bp += strlen(bp);
		j = i;
		for (++i; i <= mp->hghmsg && (mp->msgstats[i] & mask) == mask;
		     ++i)
			;
		if (i - j > 1) {
			(void) sprintf(bp, "-%s", m_name(i - 1));
			bp += strlen(bp);
		}
		linlen += bp - oldbp;
	}
	return (bp > buffer? buffer : NULL);
}
