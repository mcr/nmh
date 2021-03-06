#include "mh.h"

int  convdir;
struct msgs *mp;
char *delimp;

m_convert(name)
{
	register char *cp;
	register int first, last;
	int found, group, range;
	char *bp;

	found = group = 0;
	if(equal((cp = name), "all"))
		cp = "first-last";
	if((first = m_conv(cp)) == -1)
		goto badbad;
	if(*(cp = delimp) && *cp != '-' && *cp != ':')  {
	baddel: printf("Illegal argument delimiter: \"%c\"\n", *delimp);
		return(0);
	}
	if(*cp == '-') {
		group++;  cp++;
		if((last = m_conv(cp)) == -1 || last < first ) {
	  badbad:       if(

			printf("Bad message list \"%s\".\n", name);
			return(0);
		}
		if(*delimp) goto baddel;
		if(first > mp->hghmsg || last < mp->lowmsg) {
	rangerr:        printf("No messages in range \"%s\".\n", name);
			return(0);
		}
		if(last > mp->hghmsg)
			last = mp->hghmsg;
		if(first < mp->lowmsg)
			first = mp->lowmsg;
	} else if(*cp == ':') {
		cp++;
		if(*cp == '-') {
			convdir = -1;
			cp++;
		} else if(*cp == '+') {
			convdir = 1;
			cp++;
		}
		if((range = atoi(bp = cp)) == 0)
			goto badbad;
		while(*bp >= '0' && *bp <= '9') bp++;
		if(*bp)
			goto baddel;
		if((convdir > 0 && first > mp->hghmsg) ||
		   (convdir < 0 && first < mp->lowmsg))
			goto rangerr;
		if(first < mp->lowmsg)
			first = mp->lowmsg;
		if(first > mp->hghmsg)
			first = mp->hghmsg;
		for(last = first; last >= mp->lowmsg && last <= mp->hghmsg;
						last =+ convdir)
			if(mp->msgstats[last]&EXISTS)
				if(--range <= 0)
					break;
		if(last < mp->lowmsg)
			last = mp->lowmsg;
		if(last > mp->hghmsg)
			last = mp->hghmsg;
		if(last < first) {
			range = last; last = first; first = range;
		}
	} else {
		if(first > mp->hghmsg || first < mp->lowmsg ||
		   !(mp->msgstats[first]&EXISTS)) {
			printf("Message %d doesn't exist.\n", first);
			return(0);
		}
		last = first;
	}
	while(first <= last) {
		if(mp->msgstats[first]&EXISTS) {
			if(!(mp->msgstats[first]&SELECTED)) {
				mp->numsel++;
				mp->msgstats[first] =| SELECTED;
				if(first < mp->lowsel)
					mp->lowsel = first;
				if(first > mp->hghsel)
					mp->hghsel = first;
			}
			found++;
		}
		first++;
	}
	if(!found)
		goto rangerr;
	return(1);
}

m_conv(str)
{
	register char *cp, *bp;
	register int i;
	char buf[16];

	convdir = 1;
	cp = bp = str;
	if(*cp >= '0' && *cp <= '9')  {
		while(*bp >= '0' && *bp <= '9') bp++;
		delimp = bp;
		return(atoi(cp));
	}
	bp = buf;
	while((*cp >= 'a' && *cp <= 'z') || *cp == '.')
		*bp++ = *cp++;
	*bp++ = 0;
	delimp = cp;
	if(equal(buf, "first"))
		return(mp->lowmsg);
	else if(equal(buf, "last")) {
		convdir = -1;
		return(mp->hghmsg);
	} else if(equal(buf, "cur") || equal(buf, "."))
		return(mp->curmsg);
	else if(equal(buf, "prev")) {
		convdir = -1;
		for(i = (mp->curmsg<=mp->hghmsg)? mp->curmsg-1: mp->hghmsg;
		    i >= mp->lowmsg; i--) {
			if(mp->msgstats[i]&EXISTS)
				return(i);
		}
	} else if(equal(buf, "next"))
		for(i = (mp->curmsg>=mp->lowmsg)? mp->curmsg+1: mp->lowmsg;
		    i <= mp->hghmsg; i++) {
			if(mp->msgstats[i]&EXISTS)
				return(i);
		}
	return(-1);
}
