#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include <stdio.h>
#include "../mh.h"
#include "../adrparse.h"
#include <ctype.h>

extern char *sprintf();

char *
adrformat(mp,hostname)
	register struct mailname *mp;
	register char *hostname;
{
	static char buf[512];
	register char *tp, *cp, *sp;

	for(tp = buf, cp = mp->m_text;*cp ;cp++ ) {
		if(cp == mp->m_at) {
			if(*cp == '@') {
				if(cp[-1] != ' ')
					*tp++ = ' ';
				*tp++ = 'a';
				*tp++ = 't';
				if(cp[1] != ' ')
					*tp++ = ' ';
				continue;
			}
		}
		if((cp == mp->m_hs) && (*mp->m_at != '!')){
			if(sp = stdhost(mp->m_hnum)) {
				while(*sp)
					*tp++ = islower(*sp) ?
						toupper(*sp++) : *sp++;
				cp = mp->m_he;
				continue;
			} else {
				fprintf(stderr, "adrformat: bad host!?\n");
				return NULL;
			}
		}
		*tp++ = *cp;
	}
	if(mp->m_nohost)
		VOID sprintf(tp, " at %s", !mp->m_at? mp->m_host: hostname);
		/* This probably only needs "hostname" unconditionally */
	else
		*tp = 0;
	return buf;
}


char *
stdhost(num)
	long num;
{
	register struct hosts *hp;

	for(hp = hosts.nh_next; hp; hp = hp->nh_next)
		if(num == hp->nh_num)
			return hp->nh_name;
	return 0;
}
