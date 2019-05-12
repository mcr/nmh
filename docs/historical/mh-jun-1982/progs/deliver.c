#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include <whoami.h>
#include "../mh.h"
#include "../adrparse.h"
#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <signal.h>
#include <strings.h>
#include <mailsys.h>
#include <time.h>


time_t time();
char *malloc(), *calloc(), *getncpy(), *sprintf(), *strcpy(), *strncop();
char *ctime();
long lseek();

#define OUTPUTLINELEN 72
#define EVERYONE 10
#define RECVPROG "/.mh_receive"
#define DAEMON_GRP 2
#define FCCS    10      /* Max number of fcc's allowed */

/* Alias */
#define STAR 2
int alitype;    /*** 0 = nomatch, 2 = *match, 1 = stringmatch ***/

struct shome {          /* Internal name/uid/home database */
	struct shome *h_next;
	char *h_name;
	int   h_uid;
	int   h_gid;
	char *h_home;
} *homes, *home();

struct swit switches[] = {
	"debug",         -5,      /* 0 */
	"deliver",       -1,      /* 1 */
	"format",         0,      /* 2 */
	"noformat",       0,      /* 3 */
	"msgid",          0,      /* 4 */
	"nomsgid",        0,      /* 5 */
	"remove",         0,      /* 6 */
	"noremove",       0,      /* 7 */
	"verbose",        0,      /* 8 */
	"noverbose",      0,      /* 9 */
	"width",          0,      /*10 */
	"help",           4,      /*11 */
	0,                0
};

int     verbose, format=1, msgid, debug, myuid, rmflg;
short   lockwait;       /* Secs to wait for mail lock (From strings/lockdir.c) */
#define LOCKWAIT (lockwait * 5) /* Ignore lock if older than this */
short   donecd;
short   outputlinelen = OUTPUTLINELEN;
long    now;
char    tmpfil[32], uutmpfil[32], fccfold[FCCS][128];
short   fccind;
char   *deliverto;
char   *head_alias = "";   /* 1st match in a local mailbox's alias chain */
			   /* (If a:b, b:c, c:d then head_alias == "a") */

struct mailname localaddrs, netaddrs, uuaddrs;
struct mailname *mn_from;
char *hdrptr;

/*ARGSUSED*/
main(argc, argv)
	int argc;
	char *argv[];
{
	register char *cp, **argp;
	register struct mailname *lp;
	char buf[BUFSIZ], name[NAMESZ];
	char *msg;
	int state, compnum, fd;
	FILE *in, *out, *uuout;

	invo_name = argv[0];
#ifndef ARPANET
	format=0;		/* default to noformat if no Arpanet */
#endif
	msg = 0;
	argp = argv + 1;
	while(cp = *argp++) {
		if(*cp == '-')
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);       /* ambiguous */
				done(1);
							/* unknown */
			case -1:fprintf(stderr, "deliver: -%s unknown switch.\n", cp);
				done(1);
			case 0: verbose++; debug++; continue; /* -debug */
			case 1: if(!(deliverto = *argp++) || *deliverto == '-'){
		  missing:          fprintf(stderr, "deliver: missing arg to %s\n",
					     argp[-2]);
				    done(1);
				}
				continue;
			case 2: format = 1; continue;         /* -format */
			case 3: format = 0; continue;         /* -noformat */
			case 4: msgid = 1;  continue;         /* -msgid */
			case 5: msgid = 0;  continue;         /* -nomsgid */
			case 6: rmflg = 1;  continue;         /* -remove */
			case 7: rmflg = -1; continue;         /* -noremove */
			case 8: verbose = 1; continue;        /* -verbose */
			case 9: verbose = 0; continue;        /* -noverbose */
			case 10: if(!(cp = *argp++) || *cp == '-')
					goto missing;
				outputlinelen = atoi(cp);
				continue;
			case 11: help("deliver [switches] file",
				     switches);
				done(1);
			}
		if(msg) {
			fprintf(stderr, "Deliver: Only one message at a time!\n");
			done(1);
		} else
			msg = cp;
	}
	if(!msg) {
		fprintf(stderr, "Deliver: No Message specified.\n");
		fprintf(stderr, "Deliver: Usage: deliver [switches] file\n");
		done(1);
	}
	if(outputlinelen < 10) {
		fprintf(stderr, "deliver: Impossible width: %d\n",
				outputlinelen);
		done(1);
	}
	gethomes();
	myuid = getuid();
	if(deliverto) {
		int uid;
		if((uid = geteuid()) && uid != 1 && getegid() != DAEMON_GRP) {
/*              if(myuid != 0 && getgid() != DAEMON_GRP) {  */
			fprintf(stderr, "Deliver: -deliver switch su only.\n");
			done(1);
		}
#ifdef VMUNIX
		setpgrp(0, getpid());   /* So we don't blow away our parent */
#endif
		strcpy(tmpfil, msg);
		strcpy(uutmpfil, msg);
		if(access(tmpfil, 4) == -1) {
			fprintf(stderr, "Deliver: Can't access ");
			perror(tmpfil);
			done(1);
		}
		if(!rmflg) rmflg = -1;
		goto del1;
	}
	if(!rmflg) rmflg = 1;
	if((in = fopen(msg, "r")) == NULL) {
		fprintf(stderr, "Deliver: Can't open ");
		perror(msg);
		done(1);
	}
	VOID copy(makename("locs", ".tmp"), copy("/usr/tmp/mh/", tmpfil));
	if(!debug) {
		if((out = fopen(tmpfil, "w")) == NULL) {
			fprintf(stderr, "Can't create %s\n", tmpfil);
			done(1);
		}
		VOID chmod(tmpfil, 0744);
	} else
		out = stdout;

	VOID copy(makename("uulc", ".tmp"), copy("/usr/tmp/mh/", uutmpfil));
	if(!debug) {
		if((uuout = fopen(uutmpfil, "w")) == NULL) {
			fprintf(stderr, "Can't create %s\n", uutmpfil);
			done(1);
		}
		VOID chmod(uutmpfil, 0600);
	} else
		uuout = fopen("/dev/null","w");;

	putdate(out);           /* Tack on the date */
	putdate(uuout);           /* Tack on the date */
	for(compnum = 1, state = FLD;;) {
		state = m_getfld(state, name, buf, sizeof buf, in);
		switch(state) {

		case FLD:
		case FLDEOF:
		case FLDPLUS:
			hdrptr = 0;
			compnum++;
			if(uleq(name, "fcc")) {
				cp = buf;
				while(*cp == ' ' || *cp == '\t')
					cp++;
				if(fccind >= FCCS) {
					fprintf(stderr, "Deliver: too many fcc's.\n");
					done(1);
				}
				VOID copy(cp, fccfold[fccind]);
				if(cp = rindex(fccfold[fccind], '\n'))
					*cp = 0;
				fccind++;
				break;
			}
			hdrptr = add(buf, hdrptr);
			while(state == FLDPLUS) {
				state = m_getfld(state, name, buf, sizeof buf, in);
				hdrptr = add(buf, hdrptr);
			}
			putfmt(name, hdrptr, out);
			{	/* Do not format uucp mail */
				int tmpformat;
				tmpformat = format; format = 0;
				putfmt (name, hdrptr, uuout);
				format = tmpformat;
			}
			if(state == FLDEOF)
				goto process;
			break;

		case BODY:
		case BODYEOF:
			putfrom(out);           /* Tack on the from */
						/* But NOT on the UUCP mail */
			putmsgid(out);          /* and msg id if desired */
			putmsgid(uuout);	/* (why not?) */

			fprintf(out, "\n%s", buf);
			fprintf(uuout, "\n%s", buf);
			while(state == BODY) {
				state=m_getfld(state,name,buf,sizeof buf,in);
				fputs(buf, out);
				fputs(buf, uuout);
			}

		case FILEEOF:
			goto process;

		case LENERR:
		case FMTERR:
			fprintf(stderr, "??Message Format Error ");
			fprintf(stderr, "in Component #%d.\n", compnum);
			done(1);

		default:
			fprintf(stderr, "Getfld returned %d\n", state);
			done(1);
		}
	}
process:
	if(!debug) {
		VOID fclose(out);
		VOID fclose(uuout);
	}
	else
		printf("-----\n");
	VOID fclose(in);
del1:   if(deliverto)   {
		cinsert(deliverto);
	}
	if(debug) {
		printf("Before alias():\n"); pl();
	}

	if(localaddrs.m_next)
		alias();                        /* Map names if needed */

	for(lp = localaddrs.m_next; lp; lp = lp->m_next)
		if(home(lp->m_mbox) == NULL) {
			fprintf(stderr, "Deliver: Unknown local user: %s.\n", lp->m_mbox);
			fprintf(stderr, "Deliver: Message not delivered.\n");
			if(!debug)
				unlinktmp();
			done(1);
		}
	if(debug) {
		printf("Addrs:\n"); pl();
	}

	VOID signal(SIGINT, SIG_IGN);
	VOID signal(SIGQUIT, SIG_IGN);
	if(!debug) {                    /* Send the mail */
		if(localaddrs.m_next) {
			fd = open(tmpfil, 0);
			for(lp = localaddrs.m_next; lp; lp = lp->m_next)
				sendmail(lp->m_mbox, fd, lp->m_headali);
			VOID close(fd);
		}
		if(fccind)
			for(state = 0; state < fccind; state++)
				fcc(tmpfil, fccfold[state]);
		if(uuaddrs.m_next)
			uumail();
		if(netaddrs.m_next)
			netmail();
		unlinktmp();
	}
	done(donecd);
}


unlinktmp()
{
	if (!deliverto) /* "Deliverto" makes it same as tmpfil */
		VOID unlink (uutmpfil);         /* Even if rmflg < 0? */
	if(rmflg > 0)
		VOID unlink(tmpfil);
}



putfmt(name, str, out)
	char *name, *str;
	FILE *out;
{
	register char *cp;
	register struct mailname *mp;
	int nameoutput = 0;
	int linepos = 0;
	int len;

	while(*str == ' ' || *str == '\t') str++;
	if(uleq(name, "to") ||
	   uleq(name, "cc") ||
	   uleq(name, "bcc") ||
	   uleq(name, "reply-to") ||
	   uleq(name, "from")) {
	    while(cp = getname(str)) {
		if(!(mp = getm(cp, HOSTNAME)))
			done(1);
		if(uleq(name, "from") || uleq(name, "reply-to") ||
		   insert(mp, 
			mp->m_hnum == HOSTNUM ?
				  	     &localaddrs : 
			(*mp->m_at == '!') ? &uuaddrs :
					     &netaddrs)) {
		    if(!uleq(name, "bcc") && format) {
			if(!nameoutput) {
			    fprintf(out, "%s: ", name);
			    linepos += (nameoutput = strlen(name)+2);
			}
			cp = adrformat(mp,HOSTNAME);	
				/* Needs default name in uumail case */
			len = strlen(cp);
			if(linepos != nameoutput)
			    if(len + linepos + 2 > outputlinelen) {
				fprintf(out, ",\n%*s", nameoutput, "");
				linepos = nameoutput;
			    } else {
				fputs(", ", out);
				linepos += 2;
			    }
			fputs(cp, out);
			linepos += len;
		    }
		    if(uleq(name, "from"))
			mn_from = mp;
		} else
		    mnfree(mp);
	    }
	    if(linepos) putc('\n', out);
	    if(!format && !uleq(name, "bcc"))
		goto nofmt;
	} else {
nofmt:
	    fprintf(out, "%s: %s", name, str);
	}
}

gethomes()
{
	register struct passwd *pw;
	register struct shome *h, *ph;
	struct passwd *getpwent();

	ph = (struct shome *) &homes;
	while((pw = getpwent()) != NULL) {
		h = (struct shome *) malloc(sizeof *h);
		h->h_next = NULL;
		h->h_name = (char *) malloc((unsigned)strlen(pw->pw_name)+1);
		strcpy(h->h_name, pw->pw_name);
		h->h_uid = pw->pw_uid;
		h->h_gid = pw->pw_gid;
		h->h_home = (char *) malloc((unsigned)strlen(pw->pw_dir)+1);
		strcpy(h->h_home, pw->pw_dir);
		ph->h_next = h;
		ph = h;
	}
}


struct shome *
home(name)
	register char *name;
{
	register struct shome *h;

	for(h = homes; h; h = h->h_next)
		if(uleq(name, h->h_name))
			return(h);
	return(NULL);
}

char *bracket = "\1\1\1\1\n";

sendmail(name, fd, head_alias)              /* Runs with signals ignored! */
	char *name;
	register int fd;
	char *head_alias;
{
	char buf[BUFSIZ];
	register int i, m, c;
	register char *mail, *receive, *lock = 0;
	register struct shome *h;
	struct stat stbuf;

	if(verbose)  {
		printf("%s: ", name);   fflush(stdout);
	}
	VOID lseek(fd, 0l, 0);
	h = home(name);

	mail = concat(mailboxes, "/", h->h_name, NULLCP);
/***    mail = concat(h->h_home, mailbox, NULLCP);           ***/
	receive = concat(h->h_home, RECVPROG, NULLCP);
	if(access(receive, 1) == 0) {   /* User has a receive prog */

		calluserprog(receive, mail, fd, h, head_alias);

	} else {

		if((m = open(mail, 2)) < 0) {
			if((m = creat(mail, 0600)) < 0) {
				fprintf(stderr, "Deliver: Can't write ");
				perror(mail);
				donecd = 1; goto out;
			}
			VOID chown(mail, h->h_uid, h->h_gid);
			if(verbose) {
				printf("Creating %s ", mail); fflush(stdout);
			}
		}
		lock = concat(lockdir, "/", h->h_name, NULLCP);
		for(i = 0; i < lockwait; i += 2) {
			if(link(mail, lock) >= 0)
				break;
			if(i == 0 && stat(mail, &stbuf) >= 0 &&
			   stbuf.st_ctime + LOCKWAIT < time((long *) 0)) {
				i = lockwait;
				break;
			}
			if(verbose) {
				printf("Busy ");  fflush(stdout);
			}
			sleep(2);
		}
		if(i >= lockwait) {
			VOID unlink(lock);
			if(verbose) {
				printf("Removing lock. ");  fflush(stdout);
			}
			if(link(mail, lock) < 0) {
				fprintf(stderr, "Can't lock %s to %s\n",
					mail, lock);
				donecd = 1;
				goto out1;
			}
		}
		VOID lseek(m, 0l, 2);
		if(write(m, bracket, 5) != 5)
			goto wrerr;
		do
			if((c = read(fd, buf, sizeof buf)) > 0)
				if(write(m, buf, c) != c) {
			    wrerr:      fprintf(stderr, "Write error on ");
					perror(mail);
					donecd = 1;
					goto out1;
				}
		while(c == sizeof buf);
		if(write(m, bracket, 5) != 5)
			goto wrerr;
out1:           VOID close(m);
out:            cndfree(mail);
		if(lock) {
			VOID unlink(lock);
			cndfree(lock);
		}
	}
	if(verbose && !donecd)
		printf("Sent.\n");
}

netmail()
{
	register struct mailname *mp;
	register struct shome *h;
	register struct hosts *hp;
	register FILE *md;
	int fd, count;
	char namebuf[15], tmpname[40], queuename[40];
	char buf[BUFSIZ];

	crname(namebuf);
	VOID sprintf(tmpname,   "%s/%s", TMAILQDIR, namebuf);
	VOID sprintf(queuename, "%s/%s",  MAILQDIR, namebuf);
	if((md = fopen(tmpname, "w")) == NULL) {
		fprintf(stderr, "Deliver: Can't create netmail tmp: ");
		perror(tmpname);
		done(1);
	}
	for(h = homes; ; h = h->h_next) {
		if(!h) {
			fprintf(stderr, "Deliver: Who are you?\n");
			done(1);
		}
		if(h->h_uid == myuid)
			break;
	}
	if(!now)
		now = time((long *)0);
	fprintf(md, "%s %s\n", h->h_name, cdate(&now));
	if(debug) {
		printf("Netmail...");   fflush(stdout);
	}
	for(mp = netaddrs.m_next; mp; mp = mp->m_next) {
		if(debug) printf("%s at %s\n",mp->m_mbox, mp->m_host);
		for(hp = hosts.nh_next; hp; hp = hp->nh_next)
			if(mp->m_hnum == hp->nh_num)
				break;
		if(!hp) {
			fprintf(stderr, "Deliver: hnum->name botch.\n");
			done(1);
		}
		fprintf(md, "/%s %s\n", hp->nh_name, mp->m_mbox);
		if(verbose)
			printf("%s at %s: queued\n", mp->m_mbox, mp->m_host);
	}
	putc('\n', md);
	if(fflush(md) == EOF) goto wrerr;
	if((fd = open(tmpfil, 0)) == -1) {
		fprintf(stderr, "Deliver: HuH? Can't reopen ");
		perror(tmpfil);
		done(1);
	}
	do {
		if((count = read(fd, buf, sizeof buf)) > 0) {
			if(write(fileno(md), buf, count) != count) {
		   wrerr:       fprintf(stderr, "Deliver: write error on ");
				perror(tmpname);
				VOID unlink(tmpname);
				done(1);
			}
		} else if(count < 0) {
			fprintf(stderr, "deliver: Read error on ");
			perror(tmpfil);
			VOID unlink(tmpname);
			done(1);
		}
	} while(count == sizeof buf);
	if(close(fd) == -1 || fclose(md) == EOF) goto wrerr;
	if(link(tmpname, queuename) == -1 ||
	   unlink(tmpname) == -1) {
		fprintf(stderr, "Deliver: Trouble linking tmpfile into queue ");
		perror(queuename);
	}
	VOID chmod (queuename, 0600);
}

uumail()
{
	register struct mailname *mp;
	register struct shome *h;
	register FILE *rmf;
	FILE *popen();
	int fd, count;
	char cmd[64];
	char buf[BUFSIZ];

	for(h = homes; h; h = h->h_next)
		if(h->h_uid == myuid)
			break;
	if(!h) {
		fprintf(stderr, "Deliver: Who are you?\n");
		done(1);
	}
	if(!now)
		now = time((long *)0);
	for(mp = uuaddrs.m_next; mp; mp = mp->m_next) {
		VOID sprintf(cmd, index(mp->m_mbox, '!') ?
				"uux - %s!rmail \\(%s\\)" :
				"uux - %s!rmail %s",
				mp->m_host, mp->m_mbox);
		if ((rmf=popen (cmd, "w")) == NULL) {
			fprintf(stderr, "Deliver: Could not open uux pipe.\n");
			done(1);
		}
	
		if((fd = open(uutmpfil, 0)) == -1) {
			fprintf(stderr, "Deliver: HuH? Can't reopen ");
			perror(uutmpfil);
			done(1);
		}
	/* UUCP mail insists on beginning with a "From" line.  */
	/*   No other concessions (">" before From) will be provided now */
		fprintf(rmf, "From %s %.24s remote from %s\n", h->h_name,
				ctime(&now),
				sysname);
		VOID fflush(rmf);    /* Lest the "write"s get in ahead */

		do {
			if((count = read(fd, buf, sizeof buf)) > 0) {
				if(write(fileno(rmf), buf, count) != count) {
		   wrerr:       	fprintf(stderr, "Deliver: write error on pipe\n");
					done(1);
				}
			} else if(count < 0) {
				fprintf(stderr, "deliver: Read error on ");
				perror(uutmpfil);
				done(1);
			}
		} while(count == sizeof buf);
		if(close(fd) == -1 ) goto wrerr;
		pclose(rmf);
		if(verbose)
			printf("%s At %s (via uux): queued.\n",
				mp->m_mbox, mp->m_host);

	}
}

char hex[] = "0123456789ABCDEF";  /* Hexadecimal */

crname(ptr)     /* Create unique file name in /usr/tmp */
	char *ptr;
{
	int i;
	short tvec[4];
	static int filecnt;
	register char *p, *q;

	q = ptr;
	p = (char *)&tvec[0];
	tvec[2] = getpid();
	tvec[3] = filecnt++;
	if (filecnt==256) {
		filecnt = 0;
		sleep(1);
	}
	VOID time((time_t *)tvec);
	for (i=7; i; --i) {
		*q++ = hex[(*p>>4)&017];
		*q++ = hex[ *p++  &017];
	}
	*q = '\0';
}


putfrom(out)
	register FILE *out;
{
	register struct shome *h;
	register struct mailname *mp;
	register char *cp;

	for(h = homes; h; h = h->h_next)
		if(h->h_uid == myuid) {
			if(format) {
				if(!(mp = getm(h->h_name, HOSTNAME)))
					done(1);
				cp = adrformat(mp,HOSTNAME);
				mnfree(mp);
			} else
				cp = h->h_name;
			if(mn_from) {
				if(mn_from->m_hnum != HOSTNUM ||
				   !uleq(mn_from->m_mbox, h->h_name))
					fprintf(out, "Sender: %s\n", cp);
			} else
				fprintf(out, "From: %s\n", cp);
			return;
		}
	fprintf(stderr, "Deliver: WHO ARE YOU?\n");
	done(1);
}


putdate(out)
	register FILE *out;
{
	register char *t, *p;
	char *timezone();
	struct timeb tb;
	struct tm *tmp, *localtime();
	static char *wday[] = {
		"Sun",
		"Mon",
		"Tues",
		"Wednes",
		"Thurs",
		"Fri",
		"Satur"
	};

	if(!now)
		now = time((long *)0);
	t = ctime(&now);
	ftime(&tb);
	tmp = localtime(&now);
	p = timezone(tb.timezone, tmp->tm_isdst);

	/*                  day    13   Apr  1981 20  :38   -PST */
	fprintf(out, "Date: %sday, %.2s %.3s %.4s %.2s:%.2s-%.3s\n",
		     wday[tmp->tm_wday], t+8, t+4, t+20, t+11, t+14, p);
}


putmsgid(sp)
	FILE *sp;
{
	if(!msgid)
		return;
	if(!now)
		now = time((long *)0);
	fprintf(sp, "Message-ID: <%u.%ld@%s>\n", myuid, now, HOSTNAME);
}


/***/
pl()
{
	register struct mailname *mp;

	printf("local: ");
	for(mp = localaddrs.m_next; mp; mp=mp->m_next)
		printf("%s (%s)%s",
		mp->m_mbox, mp->m_headali, mp->m_next?", ":"");
	printf("\n");
	printf("net: ");
	for(mp = netaddrs.m_next; mp; mp=mp->m_next)
		printf("%s@%s (%s)%s",
		 mp->m_mbox, mp->m_host, mp->m_headali, mp->m_next?", ":"");
	printf("\n");
	printf("uucp: ");
	for(mp = uuaddrs.m_next; mp; mp=mp->m_next)
		printf("%s!%s (%s)%s",
		mp->m_host, mp->m_mbox, mp->m_headali, mp->m_next?", ":"");
	printf("\n");
}
/***/

insert(np, queue)
	register struct mailname *np, *queue;
{
	register struct mailname *mp;

	/*** printf("insert(%s@%d)=>", np->m_mbox, np->m_hnum);   /***/
	for(mp = queue; mp->m_next; mp = mp->m_next)
		if(duplicate(np, mp)) { /* Don't insert existing name! */
			/*** printf("0\n");     /***/
			return 0;
		}
	mp->m_next = np;
/*** printf("1\n");     /***/
	return 1;
}

duplicate(np, mp)
struct mailname *np, *mp;
{
	if (*np->m_at == '!')
		/* If uucp, compare host names */
		if(uleq(np->m_host, mp->m_next->m_host) &&
		   uleq(np->m_mbox, mp->m_next->m_mbox))
			return(1);
		else
			return(0);

	/* Otherwise, compare host numbers */
	if(np->m_hnum == mp->m_next->m_hnum &&
	uleq(np->m_mbox, mp->m_next->m_mbox))
		/* Special consideration, eg:  news.topic1 != news.topic2 */
		return( uleq(np->m_headali, mp->m_next->m_headali) ? 1
		: alitype == STAR ?  0 : 1);
	else
		return(0);
}



cinsert(str)
	char *str;
{
	register struct mailname *mp;

	if(!(mp = getm(str, HOSTNAME)))
		done(1);
	mp->m_headali =  getcpy(head_alias);
	if(!insert(mp, mp->m_hnum == HOSTNUM ?
					&localaddrs : 
			(*mp->m_at == '!')?  &uuaddrs :
					&netaddrs)) {
		mnfree(mp);
	}
}


/* alias implementation below...
 */

#define SAVE   0
#define RESET  1
#define NEXT   2
#define GROUP   "/etc/group"
char    *AliasFile =    "/etc/MailAliases";

char *termptr, *listsave;

char *
parse(ptr, buf)
	register char *ptr;
	char *buf;
{
	register char *cp;

	cp = buf;
	while(isspace(*ptr) || *ptr == ',' || *ptr == ':')
		ptr++;
	while(isalnum(*ptr) || *ptr == '/' || *ptr == '-' ||
	    *ptr == '!' || *ptr == '@' || *ptr == ' ' ||
	    *ptr == '.' || *ptr == '*')
		*cp++ = *ptr++;
	if(cp == buf) {
		switch(*ptr) {
		case '<':
		case '=':
			*cp++ = *ptr++;
		}
	}
	*cp = 0;
	if(cp == buf)
		return 0;
	termptr = ptr;
	return buf;
}

char *
setptr(type)
int type;
{
	switch(type) {

	case SAVE:
		listsave = termptr;
		break;
	case RESET:             /* Reread the current alias replacement-list */
		termptr = listsave;
		break;
	case NEXT:
		break;
	}
	return(termptr);
}

alias()
{
	register char *cp, *pp;
	char *parsep = 0;
	struct mailname ptrsave;
	register struct mailname *lp;
	char line[256], pbuf[64];
	FILE *a;

	if((a = fopen(AliasFile, "r")) == NULL) {
		fprintf(stderr, "Can't open alias file ");
		perror(AliasFile);
		done(1);
	}
	while(fgets(line, sizeof line, a)) {
		if(line[0] == ';' || line[0] == '\n')   /* Comment Line */
			continue;
		cndfree(parsep);
		if((parsep = getcpy(parse(line, pbuf))) == NULL) {
	    oops:       fprintf(stderr, "Bad alias file %s\n", AliasFile);
			fprintf(stderr, "Line: %s", line);
			done(1);
		}
		VOID setptr(SAVE);
		for(lp = &localaddrs; lp->m_next; lp = lp->m_next) {
			if(alitype = aleq(lp->m_next->m_mbox, parsep)) {
				setali(lp->m_next);

				ptrsave.m_next = lp;   /* Maintain ptr */
				remove(lp);            /* continuity after */
				lp = &ptrsave;         /* lp->next is removed. */

				if(!(cp = setptr(NEXT)) ||
				   !(pp = parse(cp, pbuf)))
					goto oops;
				switch(*pp) {
				case '<':       /* From file */
					cp = setptr(NEXT);
					if((pp = parse(cp, pbuf)) == NULL)
						goto oops;
					addfile(pp);
					break;
				case '=':       /* UNIX group */
					cp = setptr(NEXT);
					if((pp = parse(cp, pbuf)) == NULL)
						goto oops;
					addgroup(pp);
					break;
				case '*':       /* ALL Users */
					addall();
					break;
				default:        /* Simple list */
					for(;;) {
						cinsert(pp);
						if(!(cp = setptr(NEXT)) ||
						   !(pp = parse(cp, pbuf)))
							break;
					}
					VOID setptr(RESET);
				}
				/* May be more news.*<topics> in hdr */
				if(alitype == STAR) continue;
				else                 break;
			}
		}
	}
	alitype = 0;
}

setali(mp)
struct mailname *mp;
{
	/* propogate original alias match */

	cndfree(head_alias);
	head_alias = (*mp->m_headali ? getcpy(mp->m_headali) :
				       getcpy(mp->m_mbox));
}


addfile(file)
	char *file;
{
	register char *cp, *pp;
	char line[128], pbuf[64];
	FILE *f;

/***    printf("addfile(%s)\n", file);          ***/
	if((f = fopen(file, "r")) == NULL) {
		fprintf(stderr, "Can't open ");
		perror(file);
		done(1);
	}
	while(fgets(line, sizeof line, f)) {
		cp = line;
		while(pp = parse(cp, pbuf)) {
			cinsert(pp);
			cp = setptr(NEXT);
		}
	}
	VOID fclose(f);
}

addgroup(group)
	char *group;
{
	register char *cp, *pp;
	int found = 0;
	char line[128], pbuf[64], *rindex();
	FILE *f;

/***    printf("addgroup(%s)\n", group);        ***/
	if((f = fopen(GROUP, "r")) == NULL) {
		fprintf(stderr, "Can't open ");
		perror(GROUP);
		done(1);
	}
	while(fgets(line, sizeof line, f)) {
		pp = parse(line, pbuf);
		if(strcmp(pp, group) == 0) {
			cp = rindex(line, ':');
			while(pp = parse(cp, pbuf)) {
				cinsert(pp);
				cp = setptr(NEXT);
			}
			found++;
		}
	}
	if(!found) {
		fprintf(stderr, "Group: %s non-existent\n", group);
		done(1);
	}
	VOID fclose(f);
}

addall()
{
	register struct shome *h;

/***    printf("addall()\n");                   ***/
	for(h = homes; h; h = h->h_next)
		if(h->h_uid >= EVERYONE)
			cinsert(h->h_name);
}

remove(mp)              /* Remove NEXT from argument node! */
	register struct mailname *mp;
{
	register struct mailname *rp;

	rp = mp->m_next;
	mp->m_next = rp->m_next;
	cndfree(rp->m_mbox);
	cndfree(rp->m_host);
	cndfree(rp->m_text);
	cndfree(rp->m_headali);
	cndfree( (char *)rp);
}

int     alarmed;

alarmclock()
{
	alarmed++;
}

char    **environ;
char    *empty[] = {0};

calluserprog(prog, mail, fd, h, head_alias)
	char *prog, *mail, *head_alias;
	int fd;
	register struct shome *h;
{
	register int pid, child, i;
	int status;

	if(verbose) {
		printf("Invoking %s ", prog);  fflush(stdout);
	}
	i = 0;
	while((child = fork()) == -1)
		if(++i > 10) {
			fprintf(stderr, "Can't get a fork to invoke %s!\n",
				prog);
			donecd = 1;
			return;
		} else
			sleep(2);
	if(child == 0) {                /* In child... */
		if(fd != 3)
			dup2(fd, 3);
		for(i = 4; i < 15; i++)
			VOID close(i);
		environ = empty;
		putenv("USER", h->h_name);
		putenv("HOME", h->h_home);
		VOID setgid(h->h_gid);
		VOID setuid(h->h_uid);
		execlp(prog, prog, tmpfil, mail, h->h_home, head_alias, 0);
		perror(prog);
		done(-1);
	}
	VOID signal(SIGALRM, alarmclock);
	alarmed = 0;
	VOID alarm(120);                 /* Give receive proc 120 secs */
	status = 0;
	while((pid = wait(&status)) != -1 && pid != child && !alarmed) ;
	if(alarmed) {
		VOID kill(0, SIGINT);
		VOID signal(SIGALRM, alarmclock);
		alarmed = 0;
		VOID alarm(120);
		while((pid = wait(&status)) != -1 && pid != child && !alarmed) ;
		if(alarmed) {
			VOID kill(child, SIGKILL);
			VOID signal(SIGALRM, alarmclock);
			alarmed = 0;
			VOID alarm(120);
			while((pid = wait(&status)) != -1 && pid != child && !alarmed) ;
		}
		fprintf(stderr, "Deliver: Killed %s--Took more than 120 seconds!\n",
			prog);
		donecd = 1;
		status = 0;
	} else
		VOID alarm(0);
	if(status) {
		printf("Deliver: %s error %d from %s on delivery to %s\n",
			status&0377? "System" : "User",
			status&0377? status&0377 : status>>8,
			prog, h->h_name);
		donecd = 1;
	}
	if(pid == -1) {
		fprintf(stderr, "Deliver: wait on receive process returned -1\n");
		perror("");
		donecd = 1;
	}
}

aleq(string, aliasent)
	register char *string, *aliasent;
{
	register int c;

	while(c = *string++)
		if(*aliasent == '*')
			return(STAR);
		else if((c|040) != (*aliasent|040))
			return(0);
		else
			aliasent++;
/***    return(*aliasent == 0 | *aliasent == '*');   ***/
	return(*aliasent == '*' ? STAR : *aliasent == 0 ? 1 : 0);
}

fcc(file, folder)
	char *file, *folder;
{
	int child, pid, status;
	char fold[128];

	if(verbose) {
		printf("Fcc: %s...", folder);   fflush(stdout);
	}
	while((child = fork()) == -1) sleep(5);
	if(child == 0) {
		if(*file != '+')
			strcpy(fold, "+");
		strcat(fold, folder);
		VOID setuid(myuid);
		execl(fileproc, "file", "-link", "-file", file, fold, 0);
		exit(-1);
	} else while((pid = wait(&status)) != -1 && pid != child) ;
	if(status)
		fprintf(stderr, "Deliver: Error on fcc to %s\n", folder);
	else if(verbose)
		p