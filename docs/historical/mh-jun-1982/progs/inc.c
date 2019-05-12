#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <strings.h>
#include <signal.h>
#include "scansub.h"

extern char *sprintf();

extern struct swit anoyes[];    /* Std no/yes gans array */

char    scanl[];
struct  msgs *mp;
FILE    *in, *aud;
struct  stat stbuf;
char    *locknode;
int     lockwait;       /* Secs to wait for lock-Def in strings/lockdir.c */
#define LOCKWAIT (lockwait*5) /* If lock is this old, simply ignore it! */

int timeflag;
int numflag;

extern  int errno;      /* MLW  4bsd does not have errno defined in errno.h */
extern  char _sobuf[];  /* MLW  standard out buffer */

struct  swit switches[] = {
	"audit audit-file",     0,      /* 0 */
	"ms ms-folder",         0,      /* 1 */
	"help",                 4,      /* 2 */
	"changecur",            0,      /* 3 */
	"nochangecur",          0,      /* 4 */
	"time",                 0,      /* 5 */
	"notime",               0,      /* 6 */
	"numdate",              0,      /* 7 */
	"nonumdate",            0,      /* 8 */
	0,                      0
};

/*ARGSUSED*/
main(argc, argv)
char *argv[];
{

	char *newmail, maildir[128], *folder, *from, *audfile;
	char *myname;
	int change_cur;
	register char *cp;
	register int i, msgnum;
	long now;
	char **ap;
	char *arguments[50], **argp;
	int done();
	long time();

	invo_name = argv[0];
	setbuf(stdout, _sobuf);
#ifdef NEWS
	m_news();
#endif

	change_cur = 1;				/* Default */
	from = 0; folder = 0; audfile = 0;
	cp = r1bindex(argv[0], '/');
	if((cp = m_find(cp)) != NULL) {
		ap = brkstring(cp = getcpy(cp), " ", "\n");
		ap = copyip(ap, arguments);
	} else
		ap = arguments;
	VOID copyip(argv+1, ap);
	argp = arguments;
	while(cp = *argp++) {
		if(*cp == '-')
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);       /* ambiguous */
				goto leave;
							     /* unknown */
			case -1:fprintf(stderr, "inc: -%s unknown\n", cp);
				goto leave;
			case 0: if(!(audfile = *argp++)) {   /* -audit */
      missing:  fprintf(stderr, "inc: Missing argument for %s switch\n", argp[-2]);
					goto leave;
				}
				continue;
			case 1: if(!(from = *argp++))        /* -ms */
					goto missing;
				continue;
			case 2:                              /* -help */
				help("inc [+folder]  [switches]", switches);
				goto leave;
			case 3:
				change_cur = 1;
				continue;
			case 4:
				change_cur = 0;
				continue;
			case 5: timeflag = 1;  continue; /* -time */
			case 6: timeflag = 0;  continue; /* -notime */
			case 7: numflag = 1;  continue; /* -numdate */
			case 8: numflag = 0;  continue; /* -nonumdate */
			}
		if(*cp == '+') {
			if(folder) {
				fprintf(stderr, "Only one folder at a time.\n");
				goto leave;
			} else
				folder = path(cp+1, TFOLDER);
		} else {
			fprintf(stderr, "Bad arg: %s\n", argp[-1]);
	fprintf(stderr, "Usage: inc [+folder] [-ms ms-folder] [-audit audit-file]\n");
			goto leave;
		}
	}
	if(!m_find("path")) free(path("./", TFOLDER));
	if(from)
		newmail = from;
	else {
		if((myname = getenv("USER")) == 0) {
			fprintf(stderr,
"Environment Variable \"USER\" Must be set to your login name!\n");
			done(1);
		}
		newmail = concat(mailboxes, "/", myname, NULLCP);
/***            VOID copy(mailbox, copy(mypath, newmail));           ***/
		if(stat(newmail, &stbuf) < 0 ||
		   stbuf.st_size == 0) {
			fprintf(stderr, "No Mail to incorporate.\n");
			goto leave;
		}
	}
	if(!folder) {
		folder = defalt;
		if(from && strcmp(from, "inbox") == 0) {
			cp = concat("Do you really want to convert from ",
				from, " into ", folder, "?? ", NULLCP);
			if(!gans(cp, anoyes))
				goto leave;
			cndfree(cp);
		}
	}
	VOID copy(m_maildir(folder), maildir);
	if(stat(maildir, &stbuf) < 0) {
		if(errno != ENOENT) {
			fprintf(stderr, "Error on folder ");
			perror(maildir);
			goto leave;
		}
		cp = concat("Create folder \"", maildir, "\"? ", NULLCP);
		if(!gans(cp, anoyes))
			goto leave;
		if(!makedir(maildir)) {
			fprintf(stderr, "Can't create folder \"%s\"\n", maildir);
			goto leave;
		}
	}
	if(chdir(maildir) < 0) {
		fprintf(stderr, "Can't chdir to: ");
		perror(maildir);
		goto leave;
	}
	if(!(mp = m_gmsg(folder))) {
		fprintf(stderr, "Can't read folder!?\n");
		goto leave;
	}
					/* Lock the mail file */
	if(!from) {
		VOID signal(SIGINT, done);
		cp = concat(lockdir, "/", myname, NULLCP);
		for(i = 0; i < lockwait; i += 2) {
			if(link(newmail, cp) == -1) {
				fprintf(stderr, "Mailbox busy...\n");
				if(i == 0 && stat(newmail, &stbuf) >= 0)
					if(stbuf.st_ctime + LOCKWAIT < time((long *)0)) {
						VOID unlink(cp);
						fprintf(stderr, "Removing lock.\n");
						continue;
					}
				sleep(2);
			} else {
				locknode = cp;  /* We own the lock now! */
				break;
			}
		}
		if(i >= lockwait) {
			fprintf(stderr, "Try again.\n");
			done(1);
		}
	}
	if((in = fopen(newmail, "r")) == NULL) {
		fprintf(stderr, "Can't open "); perror(newmail);
		goto leave;
	}
	if(audfile) {
		cp = m_maildir(audfile);
		if((i = stat(cp, &stbuf)) < 0)
			fprintf(stderr, "Creating Receive-Audit: %s\n", cp);
		if((aud = fopen(cp, "a")) == NULL) {
			fprintf(stderr, "Can't append to ");
			perror(cp);
			goto leave;
		} else if(i < 0)
			VOID chmod(cp, 0600);
		now = time((long *)0);
		fputs("<<inc>> ", aud);
		cp = cdate(&now);
		cp[9] = ' ';
		fputs(cp, aud);
		if(from) {
			fputs("  -ms ", aud);
			fputs(from, aud);
		}
		putc('\n', aud);
	}
	printf("Incorporating new mail into %s...\n\n", folder);
	VOID fflush(stdout);
	msgnum = mp->hghmsg;

	while((i = scan(in, msgnum+1, msgnum+1, msgnum == mp->hghmsg,
					  (timeflag ? DOTIME : 0)
					  | (numflag ? NUMDATE : 0), 0))) {
		if(i == -1) {
			fprintf(stderr, "inc aborted!\n");
			if(aud)
				fputs("inc aborted!\n", aud);
			goto leave;
		}
		if(i == -2) {
			fprintf(stderr,
				"More than %d messages. Inc aborted!\n",
				MAXFOLDER);
			fprintf(stderr,"%s not zero'd\n", newmail);
			goto leave;
		}
		if(aud)
			fputs(scanl, aud);
		VOID fflush(stdout);
		msgnum++;
	}

	VOID fclose(in);
	if(aud)
		VOID fclose(aud);

	if(!from) {
		if((i = creat(newmail, 0600)) >= 0)     /* Zap .mail file */
			VOID close(i);
		else
			fprintf(stderr, "Error zeroing %s\n", newmail);
	} else
		printf("%s not zero'd\n", newmail);

	i = msgnum - mp->hghmsg;
   /*   printf("%d new message%s\n", i, i==1? "":"s");          */
	if(!i)
		fprintf(stderr, "[No messages incorporated.]\n");
	else {
		m_replace(pfolder, folder);
		if (change_cur)
			m_setcur(mp->hghmsg + 1);
	}
leave:
	m_update();
	done(0);
}


done(status)
{
	if(locknode);
		VOID unlink(locknode);
	exit(sta