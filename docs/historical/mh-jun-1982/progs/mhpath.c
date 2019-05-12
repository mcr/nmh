#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>
#include <strings.h>


struct msgs *mp;
extern  char _sobuf[];

struct swit switches[] = {
	"all",         -3,      /* 0 */
	"help",         4,      /* 1 */
	0,              0
};

/*ARGSUSED*/
main(argc, argv)
	int argc;
	char *argv[];
{
	char *cp;
	char *mhnam();

	if(!(cp = mhnam(&argv[1])))
		done(1);
	printf("%s\n", cp);     /* Expanded message list */
	done(0);
}

char *
mhnam(args)
	char **args;
{
	int j;
	static char *string;
	char buf[100];
	char *folder, *maildir, *msgs[100];
	register int msgnum;
	register char *cp;
	int msgp;
	char *arguments[50], **argp;

	setbuf(stdout, _sobuf);

	folder = (char *) 0;
	msgp = 0;
	VOID copyip(args, arguments);
	argp = arguments;
	while(cp = *argp++) {
		if(*cp == '-')
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);       /* ambiguous */
				return(0);
							/* unknown */
			case -1:fprintf(stderr, "mhnam: -%s unknown\n", cp);
				return(0);
							 /* -all */
			case 0: fprintf(stderr, "\"-all\" changed to \"all\"\n");
				return(0);
			case 1: help("mhnam [+folder]  [msgs] [switches]", switches);
				return(0);
			}
		if(*cp == '+') {
			if(folder) {
				fprintf(stderr, "Only one folder at a time.\n");
				return(0);
			} else
				folder = path(cp+1, TFOLDER);
		} else
			msgs[msgp++] = cp;
	}
	if(!m_find("path")) free(path("./", TFOLDER));

	/* Mhpath defaults to the folder name     */
	/***    if(!msgp)
	 ***            msgs[msgp++] = "cur";
	 ***/

	if(!folder)
		folder = m_getfolder();
	maildir = m_maildir(folder);


	if(!msgp)
		return(maildir);


	if(chdir(maildir) < 0) {
		fprintf(stderr, "Can't chdir to: ");
		perror(maildir);
		return(0);
	}

	if(!(mp = m_gmsg(folder))) {
		fprintf(stderr, "Can't read folder!?\n");
		return(0);
	}
	/* Need to accomodate MAXFOLDER messages instead of mp->hghmsg */
	if( (char *) (mp = (struct msgs *)
	     realloc((char *) mp, (unsigned)(sizeof *mp + MAXFOLDER + 1 + 2)))
	   == (char *) 0)
		return(0);

	/* Clear the newly allocated space */
	for(j = mp->hghmsg + 1; j <= MAXFOLDER; j++)
		mp->msgstats[j] = 0;

	/* Mhpath permits empty folders */
	/***    if(mp->hghmsg == 0) {
	 ***            fprintf(stderr, "No messages in \"%s\".\n", folder);
	 ***            return(0);
	 ***    }
	 ***/

	if(msgp)
		for(msgnum = 0; msgnum < msgp; msgnum++)
			if(!convert(msgs[msgnum]))
				return(0);
	if(mp->numsel == 0) {
		fprintf(stderr, "mhnam: Never get here. \n");
		return(0);
	}
	if(mp->numsel > MAXARGS-2) {
		fprintf(stderr, "mhnam: more than %d messages \n", MAXARGS-2);
		return(0);
	}

	for(msgnum= mp->lowsel; msgnum<= mp->hghsel; msgnum++)
		if(mp->msgstats[msgnum]&SELECTED)
		{
			VOID sprintf(buf,"%s%c%s", maildir,'/', m_name(msgnum));

			if(string)
				string = add("\n", string);
			string = add(buf, string);
		/***    printf("buf %s  string %s\n",buf,string); ***/
		}
	return(string);
}


#include <ctype.h>

int  convdir;
char *delimp;

#define FIRST 1                                         /***/
#define LAST  2                                         /***/

convert(name)       /*** Slightly hacked version of ../subs/m_convert.c */
char *name;
{
	register char *cp;
	register int first, last;
	int found, range, err;
	char *bp;

	found = 0;


	if(strcmp((cp = name), "new") == 0)             /***/
	{
		if((err = first = getnew(cp)) <= 0)
			goto badbad;
		goto single;
	}
	if(strcmp((cp = name), "all") == 0)
		cp = "first-last";
	if((err = first = conv(cp, FIRST)) <= 0)   /***/
		goto badbad;
	if(*(cp = delimp) && *cp != '-' && *cp != ':')  {
	baddel: fprintf(stderr, "Illegal argument delimiter: \"%c\"\n", *delimp);
		return(0);
	}
	if(*cp == '-') {
		cp++;
		if((err = last = conv(cp, LAST)) <= 0) {    /***/
	  badbad:       if(err == -1)
				fprintf(stderr, "No %s message\n", cp);
			else if (err == -2)                     /***/
				fprintf(stderr,
				 "Message %s out of range 1-%d\n",
				  cp,MAXFOLDER);
			else
	  badlist:              fprintf(stderr, "Bad message list \"%s\".\n",
					name);
			return(0);
		}
		if(last < first) goto badlist;
		if(*delimp) goto baddel;
		if(first > mp->hghmsg || last < mp->lowmsg) {
	rangerr:        fprintf(stderr,"No messages in range \"%s\".\n",name);
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
			goto badlist;
		while(isdigit(*bp)) bp++;
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
						last += convdir)
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
	} else  {
	      /*** Here's the hack: cur message, single numeric message,
	       *** or new message are permitted to be non-existent
	       ***/
single:         last = first;
		mp->msgstats[first] |= SELECT_EMPTY;
	}
	while(first <= last) {
		if(mp->msgstats[first]&(EXISTS|SELECT_EMPTY)) { /***/
			if(!(mp->msgstats[first]&SELECTED)) {
				mp->numsel++;
				mp->msgstats[first] |= SELECTED;
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

conv(str, callno)   /*** Slightly hacked version of ../subs/m_conv.c */
char *str;
int callno;             /***/
{
	register char *cp, *bp;
	register int i;
	char buf[16];

	convdir = 1;
	cp = bp = str;
	if(isdigit(*cp))  {
		while(isdigit(*bp)) bp++;
		delimp = bp;
/***            return (i = atoi(cp)) > MAXFOLDER ? MAXFOLDER : i;   ***/
		/* If msg # <= MAXFOLDER, return it;
		 * if > MAXFOLDER but part of a range, return MAXFOLDER;
		 * if single explicit msg #, return error.
		 */
		return (i = atoi(cp)) <= MAXFOLDER ? i :
		  *delimp || callno == LAST ? MAXFOLDER : -2;
	}
	bp = buf;
	while((*cp >= 'a' && *cp <= 'z') || *cp == '.')
		*bp++ = *cp++;
	*bp++ = 0;
	delimp = cp;
	if(strcmp(buf, "first") == 0)
		return(mp->hghmsg? mp->lowmsg : -1);    /* Folder empty? */
/***            return(mp->lowmsg);     ***/
	else if(strcmp(buf, "last") == 0) {
		convdir = -1;
		return(mp->hghmsg? mp->hghmsg : -1);
/***            return(mp->hghmsg);     ***/
	} else if(strcmp(buf, "cur") == 0 || strcmp(buf, ".") == 0)
		return(mp->curmsg > 0 ? mp->curmsg : -1);
	else if(strcmp(buf, "prev") == 0) {
		convdir = -1;
		for(i = (mp->curmsg<=mp->hghmsg)? mp->curmsg-1: mp->hghmsg;
		    i >= mp->lowmsg; i--) {
			if(mp->msgstats[i]&EXISTS)
				return(i);
		}
		return(-1);                     /* non-existent message */
	} else if(strcmp(buf, "next") == 0)  {
		for(i = (mp->curmsg>=mp->lowmsg)? mp->curmsg+1: mp->lowmsg;
		    i <= mp->hghmsg; i++) {
			if(mp->msgstats[i]&EXISTS)
				return(i);
		}
		return(-1);
	} else
		return(0);                     /* bad message list */
}

getnew(str)
char *str;
{
	register char *cp;

	return(mp->hghmsg<MAXFOLDER? mp->hghmsg+ 1 : 