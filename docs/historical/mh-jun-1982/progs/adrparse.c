#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include <stdio.h>
#include <whoami.h>
#include "../mh.h"
#include "../adrparse.h"

extern char *strcpy(), *strncpy();

#ifndef ARPANET
#define HOSTTBL "/dev/null"
#else
#include <imp.h>
#endif
#include <ctype.h>

char *malloc(), *calloc(), *getncpy();


char *
getname(addrs)
	char *addrs;
{
	register char *tp;
	static char name[256];
	static char *getaddrs;

	if(!getaddrs)
		getaddrs = addrs;
	tp = name;

	while(*getaddrs && isspace(*getaddrs))
		getaddrs++;
	if(!*getaddrs) {
		getaddrs = 0;
		return NULL;
	}
	while(*getaddrs && *getaddrs != ',') {
		if(isspace(*getaddrs))
			*tp++ = ' ';
		else
			*tp++ = *getaddrs;
		getaddrs++;
	}
	if(*getaddrs) getaddrs++;
	*tp = 0;
	while(isspace(tp[-1]) && tp > name)
		*--tp = 0;
	tp = name;
	while(*tp == ' ' || *tp == '\t')
		tp++;
	if(*tp == 0)
		return NULL;
	return tp;
}


struct mailname *
getm(str, defaulthost)		/* WHY isn't it just taken from HOSTNAME?*/
				/* Answer: REPLY uses foreign site */
	char *str;
	char *defaulthost;
{
	register char *cp;
	register char *mbp = 0, *mbe = 0;
	register char *hp = 0, *he = 0;
	register struct mailname *mp;
	int lbrkt = 0;

	if(!hosts.nh_next)
		gethosts();
	mp = (struct mailname *) calloc(1, sizeof *mp);
	mp->m_headali = "";
	mp->m_text = cp = getcpy(str);
	while(*cp) {
	    switch(*cp) {
#ifdef FOO
		case '"':
			if(!mbp) mbp = cp;
			do {
				cp++;
			} while(*cp && *cp != '"');
			break;
#endif
		case '<':
			mbp = mbe = hp = he = mp->m_at = 0;
			lbrkt++;
			break;
		case '>':
			if(!lbrkt) {
				fprintf(stderr, "adrparse: extraneous '>'\n");
				goto line;
			}
			if(mbp && !mbe) {
				fprintf(stderr, "adrparse: Missing host within <> spec.\n");
				goto line;
			} else if(hp && !he)
				he = cp - 1;
			goto gotaddr;
		case ' ': if(!HOSTNUM) break;
			if(strncmp(cp, " at ", 4) == 0 ||
			   strncmp(cp, " At ", 4) == 0 ||
			   strncmp(cp, " AT ", 4) == 0) {
				cp++;
		   at:		if (!HOSTNUM)break;	/* Host 0 means no arpanet */
				if(!mbp) {
					fprintf(stderr, "adrparse: at without mbox\n");
					goto line;
				}
				if(!mbe)
					mbe = cp - 1;
				if(mp->m_at) {
				    if (*mp->m_at == '!')
					mbp = hp;     /* uusite!person@site */
				    else
					mbe = cp - 1;   /* [x!]y@q@z */
				}
				mp->m_at = cp;
				if(*cp != '@')
					cp += 2;
				hp = he = 0;
			}
			break;
		case '@':
			goto at;
		case '!':				/* uusite!otherstuff */
			if (!mbp && !hp) break;	/* Ignore leading '!'s */
			if(mbp && !hp) {	/* No other '!'s so far...*/
				hp=mbp;			/* Host name */
				he=cp-1;

				mbp = 0;
				mp->m_at = cp;
			}
			break;
		case '(':
			if(mbp && !mbe)
				mbe = cp - 1;
			else if(hp && !he)
				he = cp - 1;
			while(*cp && *cp != ')') cp++;
			break;
		default:
			if(isalnum(*cp) || *cp == '-' || *cp == '.' ||
			    *cp == '_') {
				if(!mbp)
					mbp = cp;
				else if(mp->m_at && !hp)
					hp = cp;
			} else {
				fprintf(stderr, "adrparse: address err: %s\n", cp);
				goto line;
			}
			break;
	    }
	    cp++;
	}
gotaddr:
	if(mbp && !mbe)
		mbe = cp - 1;
	else if(hp && !he)
		he = cp - 1;
	if(!mp->m_at) {
		if(hp) {
			fprintf(stderr, "adrparse: HUH? host wo @\n");
			return(0);
		}
		mp->m_nohost++;
		if(defaulthost == 0) {
			fprintf(stderr, "adrparse: Missing host\n");
			return(0);
		}
		mp->m_host = getcpy(defaulthost);
	} else {
		while(*he == ' ') --he;
		mp->m_host = getncpy(hp, he-hp+1);
		mp->m_hs = hp;
		mp->m_he = he;
		if (*mp->m_at == '!')	/* It's a uucp addr, not arpa */
			mp->m_nohost++;	/* So formatter will glue on local name*/
	}
	if(!mbp) {
		fprintf(stderr, "adrparse: No mailbox: %s\n", str);
		return(0);
	}
	while(*mbe == ' ') --mbe;
	mp->m_mbox = getncpy(mbp, mbe-mbp+1);
	if (*mp->m_at == '!')		/* Going out over UUCP */
	{
		mp->m_hnum = -1;	/* UUCP addresses are basically local*/
		return mp;
	}
#ifndef ARPANET
	mp->m_hnum = 0;
	return mp;
#else
	if((mp->m_hnum = gethnum(mp->m_host)) == -1) {
		fprintf(stderr, "adrparse: Unknown host: %s\n", mp->m_host);
#endif
      line:     fprintf(stderr, "adrparse: In address: %s\n", str);
		return(0);
#ifdef ARPANET
	}
	if(mp->m_at && (mp->m_hnum == HOSTNUM))
		/* Local!  Try to reparse in case of UUCP address */
	{
		char localname[32];

		strcpy(localname, mp->m_mbox);	/* not same struct */
		mnfree(mp);
		if((mp = getm(localname,HOSTNAME)) == 0)  /* Really local */
			return(0);
	}
	return mp;
#endif
}


char *
getncpy(str, len)
	char *str;
	int len;
{
	register char *cp;

	cp = calloc(1, (unsigned)len + 1);
	strncpy(cp, str, len);
	return cp;
}


gethosts()
{
	register FILE *ht;
	register struct hosts *hp = &hosts;
	char buf[32], hostname[16];
	int hn;

	if((ht = fopen(HOSTTBL, "r")) == NULL) {
hte:            perror(HOSTTBL);
		exit(1);
	}
	while(fgets(buf, sizeof buf, ht)) {
		if(sscanf(buf, "%o %s", &hn, hostname) != 2)
			goto hte;
		hp->nh_next = (struct hosts *) calloc(1, sizeof *hp);
		hp = hp->nh_next;
		hp->nh_name = getcpy(hostname);
		hp->nh_num = hn;
	}
	VOID fclose(ht);
}

long
gethnum(host)
	char *host;
{
	register struct hosts *hp;

	for (hp = hosts.nh_next; hp; hp = hp->nh_next)
		if(uleq(host, hp->nh_name))
			return hp->nh_num;
	return -1;
}


mnfree(mn)
	register struct mailname *mn;
{
	free(mn->m_mbox);
	free(mn->m_host);
	free(mn->m_text);
	cndfree(mn->m_headali);
	free((char *)mn);
}

#ifdef COMMENT
/* Eventually we should do something more like this (from tn.c) */
	if ((hnum>>24) == 0) {          /* Old format */
		netparm.no_imp = hnum&077;
		netparm.no_host = hnum>>6;
	}
	else {                          /* New format */
		netparm.no_imp = hnum&0177777;
		netparm.no_host = hnum>>16;
	}