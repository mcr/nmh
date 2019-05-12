#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

static char *sccsid = "@(#)from rmail.c      4.1 (Berkeley) 10/1/80";
/*
 * rmail: front end for mail to stack up those stupid >From ... remote from ...
 * lines and make a correct return address.  This works with the -f option
 * to /etc/delivermail so it won't work on systems without delivermail.
 * However, it ought to be easy to modify a standard /bin/mail to do the
 * same thing.
 *
 * NOTE: Rmail is SPECIFICALLY INTENDED for ERNIE COVAX because of its
 * physical position as a gateway between the uucp net and the arpanet.
 * By default, other sites will probably want /bin/rmail to be a link
 * to /bin/mail, as it was intended by BTL.  However, other than the
 * (somewhat annoying) loss of information about when the mail was
 * originally sent, rmail should work OK on other systems running uucp.
 * If you don't run uucp you don't even need any rmail.
 *
 * This version revised around New Year's Day 1981 to interface with
 * Rand's MH system.  The nature of the revision is to (1) do nothing
 * if the destination is uucp-remote (note the assumption of single
 * destination) and (2) to call "/etc/mh/deliver" to do the deed.
 *
 * Revised Aug 1981 to add NOGATEWAY screen.  Also fixed a few bugs.
 * PK.
 *
 * 5/8/82: Tack on uu-Date: if msg has no date.  PK.
 */

/*#define DEBUG 1*/

#include "../mh.h"
#include <whoami.h>
#include <stdio.h>

#define MATCH   0
#define PARSE   1
#define NOPARSE 0


char *index();

FILE *out;              /* output to delivermail */
char *tmpfil;           /* file name of same */
char tmpfila[24];       /* array for tmpfil */
char *to;               /* argv[1] */
char from[512];         /* accumulated path of sender */
char lbuf[512];         /* one line of the message */

char d1[10], d2[10], d3[10], d4[10], d5[10];   /*** ctime() fields ***/

#ifdef ARPANET
int netaddr;            /* set if "to" contains an arpanet address */
#endif

main(argc, argv)
char **argv;
{
	char ufrom[64];	/* user on remote system */
	char sys[64];	/* a system in path */
	char junk[512];	/* scratchpad */
	char *cp;
	int badhdr;   /***/

	to = argv[1];
	if (argc != 2) {
		fprintf(stderr, "Usage: rmail user\n");
		exit(1);
	}
#ifdef DEBUG
	out=stdout;
	tmpfil = "/dev/tty";
#else
	tmpfil = tmpfila;
	sprintf (tmpfil, "/tmp/%s", makename ("mail",".tmp"));
	if ((out=fopen(tmpfil, "w")) == NULL) {
		fprintf(stderr, "Can't create %s\n", tmpfil);
		exit(1);
	}
#endif
	for (;;) {
		fgets(lbuf, sizeof lbuf, stdin);
		if (strncmp(lbuf, "From ", 5) && strncmp(lbuf, ">From ", 6))
			break;
		fputs(lbuf, out); /* Save--in case we are just forwarding */
/***/           sscanf(lbuf, "%s %s %s %s %s %s %s remote from %s",
		 junk, ufrom, d1, d2, d3, d4, d5, sys);
/*              sscanf(lbuf, "%s %s", junk, ufrom);  */
		cp = lbuf;
		for (;;) {
			cp = index(cp+1, 'r');
			if (cp == NULL)
				cp = "remote from somewhere";
#ifdef DEBUG
			printf("cp='%s'\n", cp);
#endif
			if (strncmp(cp, "remote from ", 12) == MATCH)
				break;
		}
		sscanf(cp, "remote from %s", sys);
		strcat(from, sys);
		strcat(from, "!");
#ifdef DEBUG
		printf("ufrom='%s', sys='%s', from now '%s'\n", ufrom, sys, from);
#endif
	}
	strcat(from, ufrom);

#ifdef DEBUG
	printf("from now '%s'\n",  from);
#endif


#ifdef ARPANET
	netaddr = isarpa(to);           /* Arpanet destination?  */

#ifdef NOGATEWAY
	if(netaddr && !okhost(sys))
	{
		truncate();
		returnmail("Sorry, not an Arpanet gateway!");
		exit(0);
	}
#endif

	if (index (to, '!') && !netaddr)  {
#else
	if (index (to, '!')) {
#endif
		/* Just forwarding! */
		putmsg(NOPARSE);
		deliver(to);
		exit(0);
	}

	truncate();
	/* fprintf(out, "To: %s\n",to); */
	putfrom();

	if( !((cp = index(lbuf, ':')) &&  (cp - lbuf <  NAMESZ ))) {
		fputs("\n",out);/* Doesn't look good; terminate hdr */
		badhdr++;
	}
	putmsg(badhdr?NOPARSE:PARSE);
	deliver(to);

}


deliver(to)
	char *to;
{
	int sts,pid,waitid;

#ifdef DEBUG
	printf("%s would get called here; Delivery to: %s \n", mh_deliver,to);
	exit(0);
#endif
	fclose(out);

	if ((pid=fork()) == -1) {
		fprintf(stderr, "Cannot fork Deliver!\n");

#ifndef DEBUG   /* Extra precaution */
		unlink(tmpfil);
#endif
		exit(1);
	}
	if(pid) {
		while(((waitid = wait(&sts)) != pid) && (waitid != -1));

#ifndef DEBUG   /* Extra precaution */
		unlink(tmpfil);
#endif
		exit(0);
	}
	execl(mh_deliver, "deliver", "-deliver", to, tmpfil, 0);

	perror( "Cannot exec Deliver ");

	exit(1);

}

#ifdef ARPANET

isarpa(str)                             /* Gateway to Arpanet? */
	char *str;
{
	char *cp;

	if (index (str, '@'))
		return(1);

	for (cp = str; ;) {
		if((cp = index(cp, ' ')) == NULL)
			return(0);
		while(*cp == ' ')
			cp++;
		if ((strncmp(cp, "at ", 3) == MATCH) ||
		    (strncmp(cp, "AT ", 3) == MATCH) ||
		    (strncmp(cp, "At ", 3) == MATCH))

			return(1);
	}
}


okhost(str)             /* Host permitted to use us as an arpanet gateway? */
	char *str;
{
	register short i;

	for (i=0; rhosts[i]; i++)
		if (strcmp (str, rhosts[i]) == MATCH)
			return(1);
	return(0);
}
#endif

returnmail(message)
	char  *message;
{

#ifdef DEBUG
	printf("returnmail()\n");
#endif
	if(!from)  return;

	putdate();
	fprintf(out, "To: %s\n", from);
	fputs("\n",out);
	fputs (message, out);
	fputs("\n\n\n*--------------RETURNED MESSAGE---------------*\n\n",
	  out);

	putmsg(NOPARSE);
	deliver(from);

}


#include <sys/types.h>
#include <sys/timeb.h>
#include <time.h>

putdate()
{
	long  now;
	register char *t, *p;
	char *timezone();
	struct timeb tb;
	struct tm *tmp;
	static char *wday[] = {
		"Sun",
		"Mon",
		"Tues",
		"Wednes",
		"Thurs",
		"Fri",
		"Satur"
	};

	now = time((long *)0);
	t = ctime(&now);
	ftime(&tb);
	tmp = localtime(&now);
	p = timezone(tb.timezone, tmp->tm_isdst);

	/*                  day    13   Apr  1981 20  :38   -PST */
	fprintf(out, "Date: %sday, %.2s %.3s %.4s %.2s:%.2s-%.3s\n",
		     wday[tmp->tm_wday], t+8, t+4, t+20, t+11, t+14, p);
}


putfrom()
{
	if (strlen(from)) 
#ifdef ARPANET
		if (netaddr)
			fprintf(out, "From: %s at %s\n",from, HOSTNAME);
		else
			fprintf(out, "From: %s\n", from);
#else
		fprintf(out, "From: %s\n",from);
#endif

}


putmsg(parse)
int parse;
{
	int dateseen = 0;

	if(!parse)
		putall();
	else {
		if (uleqn(lbuf, "date:", 5) == 0)
			dateseen++;
		fputs(lbuf, out);
		while (fgets(lbuf, sizeof lbuf, stdin)) {
			if(lbuf[0] == '\n' ) { /* end of hdrs */
				if(!dateseen)
					uudate();
				putall();
			} else {
				if (uleqn(lbuf, "date:", 5) == 0)
					dateseen++;
				fputs(lbuf, out);
			}
		}
	}
}


putall()
{
	fputs(lbuf, out);
	while (fgets(lbuf, sizeof lbuf, stdin))
		fputs(lbuf, out);
}


truncate()
{
	/* Truncate those "...remote from..." header lines. */
	/* They're kept only if the message is to be uucp-forwarded */

#ifndef DEBUG
	fclose (out);   out=fopen(tmpfil, "w");
#endif

}


/*
 * Compare strings (at most n bytes) without regard to case.
 *   Returns:   s1>s2: >0,  s1==s2: 0,  s1<s2: <0.
 */

uleqn(s1, s2, n)
register char *s1, *s2;
register n;
{

	while (--n >= 0 && (*s1|040) == (*s2|040)) {
		s2++;
		if (*s1++ == '\0')
			return(0);
	}
	return(n<0 ? 0 : (*s1|040) - (*s2|040));
}


uudate()
{
	char *prefix();

	/*                  day    13   Apr  1981 20  :38   -PST */
	fprintf(out, "Date: %sday, %.2s %.3s %.4s %.2s:%.2s-%.3s\n",
		     prefix(d1), d3, d2, d5, d4, d4+3, "???");
}

char *
prefix(str)
char *str;
{
	static char *wday[] = {
		"Sun",
		"Mon",
		"Tues",
		"Wednes",
		"Thurs",
		"Fri",
		"Satur",
		0
	};

	register char **wp;

	for(wp=wday; wp; wp++)
		if(uleqn(str, *wp, 3) == 0)
			return(*wp);
	return("???");
}
