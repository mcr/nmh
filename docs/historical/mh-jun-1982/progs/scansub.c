#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/* the only thing wrong with this version is that
   the numeric date format needs to be adjusted for
   the timezone.  See adjtime() at the end of the file.
   -- dave yost, june, 1981
 */
#include "../mh.h"
#include <whoami.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include "../adrparse.h"
#include "scansub.h"

#define Block

#define _FROM    1
#define _NOTFROM 0

#define MSGN     0              /* Start of msg name field      */
#define SMSGN    DMAXFOLDER     /* Length                       */
#define SFLGS    2              /* Width of flag field          */
#define SDATE    7              /* Length of Date field         */
#define STIME    8              /* Length of time field         */
#define SNDATE   6              /* Length of numeric date       */
#define SNTIME   4              /* Length of numeric time       */
#define SFROM   16              /* Length of "        "         */
#define SLINE   79              /* size of line                 */
#define BSUBJ   20              /* Room needed in Sub field to  */
				/* add stuff from the body      */

FILE  *scnout;
char scanl[512];
int local;
int hostseen;
char *frmtok();

scan(inb, innum, outnum, curflg, howdate, header)
	FILE *inb;
	int outnum;
{

	char buf[BUFSIZ], name[NAMESZ], tobuf[32], frombuf[32];
	register char *cp, **tok1;
	int state, subsz, first, compnum;
	static char *myname;
	int f_msgn;     /* Start of msg name field      */
	int f_smsgn;    /* Length                       */
	int f_flgs;     /* Start of flags field         */
	int f_sflgs;    /* Width of flag field          */
	int f_date;     /* Start of Date field          */
	int f_sdate;    /* Length                       */
	int f_from;     /* Start of From field          */
	int f_sfrom;    /* Length of "        "         */
	int f_subj;     /* Start of Subject field       */
	int f_ssubj;    /* Size of Subject field        */
	int f_bsubj;    /* Room needed in Sub field to  */
			/* add stuff from the body      */

	f_msgn  = MSGN;
	f_smsgn = SMSGN;
	f_flgs  = f_msgn + f_smsgn;
	f_sflgs = SFLGS;
	f_date  = f_flgs + f_sflgs;

	switch (howdate) {
	default:
		f_sdate = SDATE;
		break;

	case DOTIME:
		f_sdate = SDATE + 1 + STIME;
		break;

	case NUMDATE:
		f_sdate = SNDATE;
		break;

	case DOTIME | NUMDATE:
		f_sdate = SNDATE + SNTIME;
		break;
	}
	f_from  = f_date + f_sdate + 1;
	f_sfrom = SFROM;
	f_subj  = f_from + f_sfrom + 2;
	f_ssubj = SLINE - f_subj;
	f_bsubj = BSUBJ;

	local = 0; hostseen = 0;
	if(!myname)
		myname = getenv("USER");
	tobuf[0] = 0; frombuf[0] = 0;
	first = 0;
	state = FLD;
	compnum = 1;

	if (header)
		printf ("  #   %-*sFrom              Subject        [<<Body]\n\n",
			f_from - f_date - 1,
			howdate == DOTIME ? "Date     Time" : "Date");
	for(;;) {

		state = m_getfld(state, name, buf, sizeof buf, inb);
		if(!first++ && state != FILEEOF) {      /*##*/
		    if(outnum) {
			cp = m_name(outnum);
			if(*cp == '?')       /* msg num out of range */
				return(-2);
			if((scnout = fopen(cp, "w")) == NULL) {
				fprintf(stderr, "Error creating msg ");
				perror(cp); done(-1);
			}
			VOID chmod(cp, m_gmprot());
		    }
		    sfill(scanl, sizeof scanl);
		    scanl[sizeof scanl - 1] = 0;
		    subsz = 0;
		    tobuf[0] = 0;
		}

		switch(state) {

		case FLD:
		case FLDEOF:
		case FLDPLUS:
			compnum++;
			if(uleq(name, "from"))
			    frombuf[cpyfrm(buf,frombuf,sizeof frombuf,_FROM)]
				= 0;
			else if(uleq(name, "date"))
				cpydat(buf, &scanl[f_date], f_sdate, howdate);
			else if(uleq(name, "subject") && scanl[f_subj] == ' ')
				subsz = cpy(buf, &scanl[f_subj], f_ssubj);
			else if(uleq(name, "to") && !tobuf[0])
				tobuf[
				   cpyfrm(buf,tobuf,sizeof tobuf-1,_NOTFROM)]=0;
			else if(uleq(name, "replied"))
				VOID cpy("-", &scanl[f_flgs+1], 1);
			put(name, buf, scnout);
			while(state == FLDPLUS) {
				state=m_getfld(state,name,buf,sizeof buf,inb);
				if(scnout)
					fputs(buf, scnout);
			}
			if(state == FLDEOF)
				goto putscan;
			continue;

		case BODY:
		case BODYEOF:
			compnum = -1;
			if(buf[0] && subsz < f_ssubj - f_bsubj) {
				int bodsz;
				scanl[f_subj+subsz+1] = '<';
				scanl[f_subj+subsz+2] = '<';
				bodsz= cpy(buf, scanl+f_subj+subsz+3,
				  f_ssubj-subsz-3);
				if(bodsz < f_ssubj - subsz - 3)
					scanl[f_subj+subsz+3 + bodsz] = '>';
				if(bodsz < f_ssubj - subsz - 4)
					scanl[f_subj+subsz+4 + bodsz] = '>';
				subsz = f_ssubj;
			}
			if(buf[0] && scnout) {
				putc('\n', scnout);
				fputs(buf, scnout);
				if(ferror(scnout)) {
					fprintf(stderr, "Write error on ");
					perror(m_name(outnum));done(-1);
				}
			}
	body:
			if (!scnout)
				state = FILEEOF; /* stop now if scan cmd */
			else while(state == BODY) { /*else inc, so copy body*/
				state=m_getfld(state,name,buf,sizeof buf,inb);
				if(state != FILEEOF)
					fputs(buf, scnout);
			}
			if(state == BODYEOF || state == FILEEOF) {
		  putscan:      cpymsgn(m_name(innum), &scanl[f_msgn], f_smsgn);
				tok1= brkstring(getcpy(frombuf), " ", "\n");
				if(!frombuf[0] || uleq(frombuf, myname) ||
				  (local && uleq(*tok1, myname))) {
					VOID cpy("To:", &scanl[f_from], 3);
					VOID cpy(tobuf, &scanl[f_from+3], f_sfrom-3);
				} else
					VOID cpy(frombuf, &scanl[f_from], f_sfrom);
				if(curflg)
					VOID cpy("+", &scanl[f_flgs], f_sflgs);
				trim(scanl);
				fputs(scanl, stdout);

				if(scnout) {
					VOID fflush(scnout);
					if(ferror(scnout)) {
						perror("Write error on ");
						perror(m_name(outnum));
						done(-1);
					}
					VOID fclose(scnout);
					scnout = NULL;
				}
				return(1);
			}
			break;

		case LENERR:
		case FMTERR:
			fprintf(stderr, "??Message Format Error ");
			fprintf(stderr, "(Message %d) ", outnum ? outnum :innum);/*##*/
			if(compnum < 0) fprintf(stderr, "in the Body.\n");
			else fprintf(stderr, "in Component #%d.\n", compnum);
			fprintf(stderr, "-----------------------------------------");
			fprintf(stderr, "-------------------------------------\n");
			goto badret;
		default:
			fprintf(stderr, "Getfld returned %d\n", state);


	badret:         if(outnum) {
				fputs("\n\nBAD MSG:\n", scnout);
				if(compnum < 0)
					fputs(buf, scnout);
				else {
					fputs(name, scnout);
					putc('\n', scnout);
				}
			/***    ungetc(inb);    ***/
				state = BODY;
				goto  body;

			}
			if(scnout)
				VOID fflush(scnout);
			return(-1);
		case FILEEOF:
			return(0);

		}

	}
}


trim(str)
char *str;
{
	register char *cp;

	cp = str;
	while(*cp) cp++;
	while(*--cp == ' ') ;
	cp++;
	*cp++ = '\n';
	*cp++ = 0;
}

sfill(str, cnt)
char *str;
{
	register char *cp;
	register int i;

	cp = str;  i = cnt;
	do
		*cp++ = ' ';
	while(--i);
}


put(name, buf, ip)
	char *name, *buf;
	register FILE *ip;
{
	if(ip) {
		fputs(name, ip);
		putc(':', ip);
		fputs(buf, ip);
		if(ferror(ip)) { perror("Write error");done(-1);}
	}
}


cpy(from, to, cnt)
	register char *from, *to;
	register int cnt;
{
	register int c;
	char *savto;

	savto = to;
	while(*from == ' ' || *from == '\t' || *from == '\n' || *from == '\f')
		from++;
	while(cnt--)
		if(c = *from) {
			if(c == '\t' || c == ' ' || c == '\n' || c == '\f') {
				*to++ = ' ';
				do 
					from++;
				while((c= *from)==' '||c=='\t'||c=='\n'||c=='\f');
				continue;
			} else {
				*to++ = c;
				from++;
			}
		} else
			break;
	return(to - savto);   /*Includes 1 char trailing white space, if any*/
}

struct tm *localtime();
long time();
char *findmonth();

struct date {
	char *day;
	char *month;
	char *year;
	char *timestr;
	char *zone;
	char zoneadd;
 };

cpydat(sfrom, sto, cnt, how)
	char *sfrom, *sto;
{
	register char *cp;
	register char *to;
	static struct tm *locvec;
	char frombuf[100];
	char buf[30];           /* should be char buf[cnt + 1] */
	long now;
	struct date dt;

	strncpy(frombuf, sfrom, sizeof frombuf);
	frombuf[sizeof frombuf -1] = '\0';
	if(!locvec) {
		now = time((long *)0);
		locvec = localtime(&now);
	}

	/* Collect the various fields of the date */
	for(cp = frombuf; *cp < '0' || *cp > '9'; cp++)
		if(!*cp)
			return;

	/* get the day */
	dt.day = cp;
	while (isdigit(*cp))
		cp++;
	if (*cp)
		*cp++ = '\0';

	/* get the month */
	while (*cp && !isalpha(*cp))
		cp++;
	dt.month = cp;
	while (isalpha (*cp))
		cp++;
	if (*cp)
		*cp++ = '\0';

	/* get the year */
	while (*cp && !isdigit(*cp))
		cp++;
	dt.year = cp;
	while (isdigit(*cp))
		cp++;
	if (*cp)
		*cp++ = '\0';

	/* Point timestr at the time, and remove colons, if present */
	while (*cp && !isdigit(*cp))
		cp++;
	dt.timestr = to = cp;
	for (; isdigit(*cp) || *cp == ':'; cp++)
		if (*cp != ':')
			*to++ = *cp;
	if (*cp)
		cp++;
	*to = '\0';

	/* get the time zone. */
	/* Can be alphas as in PST */
	/* If plus/minus digits, then set zoneadd to the + or - */
	/* point zone at the string of digits or alphas */
	dt.zoneadd = '\0';
	while (*cp && !isalpha(*cp) && !isdigit(*cp)) {
		if (*cp == '+' || *cp == '-')
			dt.zoneadd = *cp;
		cp++;
	}
	dt.zone = cp;
	if (isdigit(*cp))
		while (isdigit (*cp))
			cp++;
	else {
		dt.zoneadd = '\0';
		while (isalpha (*cp))
			cp++;
	}
	if (*cp)
		*cp++ = '\0';

 /*     printf ("Yr='%s' Mo='%s' Day='%s' Time='%s' Zone='%s'\n",
		dt.year, dt.month, dt.day, dt.timestr, dt.zone);
	if(cp = findmonth(dt.month))
		printf ("month='%s'\n", cp);
	return;
 /**/
	to = buf;
	if(cp = findmonth(dt.month)) {
		if (how & NUMDATE) {
			adjtime(&dt);
			if(strlen(dt.year) >= 2) {
				cp = &dt.year[strlen(dt.year) - 2];
				*to++ = *cp++;
				*to++ = *cp++;
			}
			else {
				*to++ = ' ';
				*to++ = ' ';
			}
			if ((cp = findmonth(dt.month))[1])
				*to++ = *cp++;
			else
				*to++ = '0';
			*to++ = *cp++;
			if (*(cp = dt.day)) {
				if (cp[1])
					*to++ = *cp++;
				else
					*to++ = '0';
				*to++ = *cp++;
			}
			else {
				*to++ = ' ';
				*to++ = ' ';
			}
			*to = '\0';
			if (how & DOTIME) {
				/* kludge for now */
				dt.timestr[4] = '\0';
				strcpy (to, dt.timestr);
			}
		}
		else {
			if(!cp[1])
				*to++ = ' ';
			while(*cp)
				*to++ = *cp++;
			*to++ = '/';

			cp = dt.day;
			if(!cp[1])
				*to++ = ' ';
			while(*cp >= '0' && *cp <= '9')
				*to++ = *cp++;
			Block {
				register int yr;
				if(   (   (yr = atoi(dt.year)) > 1970
				       && yr - 1900 < locvec->tm_year
				      )
				   || yr < locvec->tm_year
				  )  {
					*to++ = '/';
					*to++ = (yr - (yr < 100 ? 70 : 1970))
						% 10 + '0';
				}
				else {
					*to++ = ' ';
					*to++ = ' ';
				}
			}
			*to = '\0';
			if (how & DOTIME) {
				*to++ = ' ';
				sprintf (to, "%4.4s", dt.timestr);
				to += 4;
				if (*dt.zone) {
					*to++ = '-';
					strncpy(to, dt.zone, 3);
				}
			}
		}
	}
	else {
		cp = dt.day;
		if(!cp[1])
			*to++ = ' ';
		while(*cp)
			*to++ = *cp++;
	}
	Block {
		register int tmp;
		if (cnt < (tmp = strlen(buf)))
			tmp = cnt;
		strncpy(sto, buf, tmp);
	}
	return;
}


char    *fromp, fromdlm, pfromdlm;

cpyfrm(sfrom, sto, cnt, fromcall)
char *sfrom, *sto;
{
	register char *to, *cp;
	register int c;

	fromdlm = ' ';
	fromp = sfrom; to = sto;
	cp = frmtok();
	do
		if(c = *cp++)
			*to++ = c;
		else
			break;
	while(--cnt);
	for(;;) {
		if(cnt < 3) break;
		if(*(cp = frmtok()) == 0) break;
		if(*cp == '@' || uleq(cp, "at")) {
			cp = frmtok();
			if(uleq(cp, HOSTNAME)) {
				/* if the first "From:" host is local */
				if(fromcall != _NOTFROM && !hostseen++)
					local++;
			} else {
				*to++ = '@';
				cnt--;
				do
					if(c = *cp++)
						*to++ = c;
					else
						break;
				while(--cnt);
			}
		} else if(cnt > 4) {
			cnt--; *to++ = pfromdlm;
			do
				if(c = *cp++)
					*to++ = c;
				else
					break;
			while(--cnt);
		}
	}
	if(fromcall != _NOTFROM)
		hostseen++;
	return(to - sto);
}


char *frmtok()
{
	static char tokbuf[64];
	register char *cp;
	register int c;

	pfromdlm = fromdlm;
	cp = tokbuf; *cp = 0;
	while(c = *fromp++) {
		if(c == '\t')
			c = ' ';
		if(c == ' ' && cp == tokbuf)
			continue;
		if(c == ' ' || c == '\n' || c == ',')
			break;
		*cp++ = c;
		*cp = 0;
		if(c == '@' || *fromp == '@' || cp == &tokbuf[63])
			break;
	}
	fromdlm = c;
	return(tokbuf);
}


/*      num specific!         */

/* copy msgnam to addr, right justified */
cpymsgn(msgnam, addr, len)
char *msgnam, *addr;
{
	register char *cp, *sp;

	sp = msgnam;
	cp = &addr[len - strlen(sp)];
	if (cp < addr)
		cp = addr;
	while(*sp)
		*cp++ = *sp++;
}

char *monthtab[] = {
	"jan", "feb", "mar", "apr", "may", "jun",
	"jul", "aug", "sep", "oct", "nov", "dec",
};

char *findmonth(str)
char *str;
{
	register char *cp, *sp;
	register int i;
	static char buf[4];

	for(cp=str, sp=buf; (*sp++ = *cp++) && sp < &buf[3] && *cp != ' '; )
		continue;
	*sp = 0;
	for(i = 0; i < 12; i++)
		if(uleq(buf, monthtab[i])) {
			VOID sprintf(buf, "%d", i+1);
			return buf;
		}
	return(0);
}

struct tzone {
	char *z_nam;
	int z_hour;
	int z_min;
};
struct tzone zonetab[] = {
     {  "GMT",   0,       0,    },
     {  "NST",  -3,     -30,    },
     {  "AST",  -4,       0,    },
     {  "ADT",  -3,       0,    },
     {  "EST",  -5,       0,    },
     {  "EDT",  -4,       0,    },
     {  "CST",  -6,       0,    },
     {  "CDT",  -5,       0,    },
     {  "MST",  -7,       0,    },
     {  "MDT",  -6,       0,    },
     {  "PST",  -8,       0,    },
     {  "PDT",  -7,       0,    },
     {  "YST",  -9,       0,    },
     {  "YDT",  -8,       0,    },
     {  "HST", -10,       0,    },
     {  "HDT",  -9,       0,    },
     {  "BST", -11,       0,    },
     {  "BDT", -10,       0,    },
     {  "Z",     0,       0,    },
     {  "A",    -1,       0,    },
     {  "M",   -12,       0,    },
     {  "N",     1,       0,    },
     {  "Y",    12,       0,    },
     {  0                       }
 };

/* adjust for timezone */
adjtime(dt)
	register struct date *dt;
{
	/* what we should do here is adjust all other timezones to our own */
	return;

 /*     if (isdigit (dt->zone[0])) {
		atoi ...
		if (dt->zoneadd == '+')
			;
	}
	else {
	}
	return;
