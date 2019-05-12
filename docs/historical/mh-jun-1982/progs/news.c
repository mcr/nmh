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
#include <sys/dir.h>
#include <strings.h>

extern char *sprintf();

#define max(a,b) (a > b ? a : b)
#define YES 1
#define NO  0

#define NEWSP   "/usr/news"     /* MUST be same as login News's login dir */
#define MAXTOPICS       50      /* Enough for a while */

int     vecp, topicp;
int     topicspec;      /* topic argument(s) given */
char    *topics[MAXTOPICS+1];
char    *vec[MAXARGS];
int     fdisplay, fcheck, fupdate, fsend, freview, fbody;  /* flags */
int     frevback, fverbose, fhelp, ftopics; /* flags */
int     hit;

struct nts {
	char    t_name[16];
	int     t_num;
} nts[MAXTOPICS+1], *check();
struct nts *ntps[MAXTOPICS];
int nnts;       /* number of actual topics */

struct swit switches[] = {
	"add",          -1,     /* 0 */
	"body",         -1,     /* 1 */
	"check",        0,      /* 2 */
	"display",      0,      /* 3 */
	"review [#]",   0,      /* 4 */
	"send topic ...",0,     /* 5 */
	"topics",       0,      /* 6 */
	"update",       0,      /* 7 */
	"help",         4,      /* 8 */
	0,              0,
};

extern  char _sobuf[];          /* MLW  standard out buffer */

/*ARGSUSED*/
main(argc, argv)
	int argc;
	char *argv[];
{
	register int i;
	register char *cp, **ap;
	char *arguments[50], **argp;

	invo_name = argv[0];
	setbuf(stdout, _sobuf);
#ifdef NEWS
	m_news();
#endif
	if (chdir (NEWSP) < 0) {        /* <-- N.B. */
		fprintf(stderr, "Can't change directory to ");
		perror(NEWSP);
		done(1);
	}
	vecp = 2;
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
				done(0);
							     /* unknown */
			case -1:vec[vecp++] = --cp; continue;
			case 0:
			case 5: fsend = YES; continue;
			case 1: fbody = YES;
				vec[vecp++] = --cp; continue;
			case 2: fcheck = YES; continue;
			case 3: fdisplay = YES; continue;
			case 4: freview = YES;
				if(**argp >= '0' && **argp <= '9')
					frevback = atoi(*argp++);
				continue;
			case 7: fupdate = YES; continue;

			case 8:
				fhelp = YES;
			case 6:
				ftopics = YES;
				continue;
			}
		else
			if(vecp == 2) {
				VOID addtopic (cp, YES);
				topicspec = YES;
			}
			else
				vec[vecp++] = cp;
	}
	if (!topicspec)
		/* user didn't specify topics so give him his defaults */
		gettopics();
	getnts();
	if (fhelp) {
		help(
"news [topic ...] [switches] [switches for \"c\" or \"mail\" ]", switches);
		putchar('\n');
	}
	if (ftopics) {
		showtopics();
		done(0);
	}
	if(fsend) {
		if(!topicspec) {
			fprintf(stderr, "Usage: news -send topic [mail switches]\n");
			done(1);
		}
		for(i = 0; i < topicp; i++)
			if(check(topics[i]))
				send(topics[i]);
			else
				printf("Unknown topic: %s\n", topics[i]);
		done(0);
	}
	for(i = 0; i < topicp; i++)
		if(check(topics[i]) == NULL)
			printf("Topic: %s unknown.\n", topics[i]);
		else
			disp(topics[i], fverbose || topicspec);
	if(fcheck) {
		if(hit)
			printf(".\n");
	} else if(!hit && !topicspec && !fupdate && !fdisplay)
		nonews();
	m_update();
	done(0);
}


struct nts *
check(topic)
	char *topic;
{
	register struct nts *t;

	for(t = nts; t->t_name[0]; t++)
		if(strncmp(topic, t->t_name, DIRSIZ) == 0)
			return t;
	return 0;
}


disp(topic, argflg)
	register char *topic;
	int argflg;
{
	register struct nts *t;
	register char *cp, *np;
	register int msgnum;
	int high;
	char buf[128];

	if((t = check(topic)) == 0)
		fprintf(stderr, "HUH?\n");
	if((cp = m_find(np = concat("news-", topic, NULLCP))) == NULL)
		cp = "0";
	high = atoi(cp);
	if(fcheck) {
		if(t->t_num > high) {
			if(!hit++)
				printf("Unread news in");
			if(hit > 1)
				printf(",");
			printf(" '%s'", topic);
		}
		return;
	}
	if(fupdate) {
		if(t->t_num > high) {
			m_replace(np, getcpy(m_name(t->t_num)));
			printf("Skipping %d items in %s.\n",
				t->t_num - high, topic);
		}
		return;
	}
	if(freview)
		if(frevback)
			msgnum = max(high - frevback, 0);
		else
			msgnum = 0;
	else
		msgnum = high;
/***    msgnum = freview? frevback? high - frevback : 0 : high; */
	if(msgnum >= t->t_num) {
		if(argflg)
			printf("%s: no new news.\n", topic);
		return;
	}
	VOID sprintf(buf, "%s/%s", NEWSP, topic);
	if(chdir(buf) == -1) {
		perror(buf);
		return;
	}
	vec[1] = showproc;
	for( ; msgnum < t->t_num;) {
		cp = m_name(++msgnum);
		if(hit) {
			printf("\nPress <return> for %s:%s...", topic, cp);
			VOID fflush(stdout);
			VOID read(0, buf, sizeof buf);
		} else
			printf("News item %s:%s\n", topic, cp);
		if(msgnum > high) {
			m_replace(np, getcpy(cp));
			m_update();
		}
		putenv("mhfolder", topic);
		vec[vecp] = cp;
		putchar('\n');
		call(vec + 1);
		hit = 1;
	}
}


call(vector)
	char **vector;
{
	register int pid, child;
	int status;

	VOID fflush(stdout);
	while((child = fork()) == -1) {
		printf("No forks...\n"); VOID fflush(stdout);
		sleep(2);
	}
	if(child == 0) {
		execv(vector[0], vector);
		perror(vector[0]);
		done(1);
	}
	while((pid = wait(&status)) != -1 && pid != child) ;
	if(pid == -1 || status) {
		fprintf(stderr, "Abnormal termination from %s\n", vector[0]);
		done(1);
	}
}


send(topic)
	register char *topic;
{
	vec[0] = mailproc;
	vec[1] = concat("news.", topic, NULLCP);
	if(!fbody)
		printf("Enter text for %s\n", topic);
	call(vec);
	free(vec[1]);
}


#ifdef CUTE
char    *nons[] = {
	"No new news",
	"No news to peruse",
	"Only old news",
	"News shortage",
	"News reporters on strike",
	"Report your own news",
	"News presses broken"
};
#define NONS    (sizeof nons/ sizeof nons[0])
#endif

nonews()
{
#ifdef CUTE
#include <sys/timeb.h>
	struct timeb tb;

	ftime(&tb);
	printf("%s.\n", nons[tb.millitm % NONS]);
#else
	printf("No new news.\n");
#endif
}

showtopics()
{
	register int i;

	sortnts();
	printf("Chk Items Topics\n");
	printf("--- ----- ------\n");
	for(i = 0; i < nnts; i++)
		printf(" %s  %4d  %s\n",
		       addtopic (ntps[i]->t_name, NO) ? "*" : " ",
		       ntps[i]->t_num, ntps[i]->t_name);
	return;
}

gettopics ()
{
	register char **ap;
	register char *cp;

	VOID addtopic ("everyone", YES);
	if((cp = m_find("news-topics")) != NULL) {
		for (ap = brkstring (cp = getcpy(cp), " ", "\n")
		    ; *ap; ap++)
			VOID addtopic (*ap, YES);
	}
	return;
}

addtopic (cp, real)
char *cp;
int real;
{
	register char **cpp;

	for (cpp = topics; *cpp; cpp++)
		if (!strcmp (*cpp, cp))
			return YES; /* We have it already */
	if (real) {
		if (!strcmp ("*", cp)) {
			register int i;
			getnts ();
			for(i = 0; i < nnts; i++)
				VOID addtopic (nts[i].t_name, YES);
		}
		else if (topicp < MAXTOPICS - 1)
			topics[topicp++] = cp;
	}
	return NO;     /* We don't have it already */
}

getnts()
{
	/* shouldn't ALWAYS look up all news folders.
	 * Should only do that for a -topics or -help.
	 */
	struct direct dir;
	struct stat st;
	register struct nts *t;
	register FILE *d;
	char tbuf[DIRSIZ + 2];

	if (nnts > 0)
		/* we've done it already */
		return;
	t = nts;
	if((d = fopen(".", "r")) == NULL) {
		fprintf(stderr, "Can't open ");
		perror(NEWSP);
		done(1);
	}
	tbuf[0] = '.';
	while(fread((char *)&dir, sizeof dir, 1, d)) {
		if(dir.d_ino && dir.d_name[0] != '.') {
			strncpy(t->t_name, dir.d_name, DIRSIZ);
			strcpy(&tbuf[1], t->t_name);
			if(stat(tbuf, &st) != -1)
				t->t_num = st.st_size;
			nnts++;
			t++;
		}
	}
	VOID fclose(d);
	return;
}

sortnts()
{
	register int i;
	extern int ntcmp();

	for (i = 0; i < nnts; i++)
		ntps[i] = &nts[i];
	qsort ((char *) ntps, nnts, sizeof ntps[0], ntcmp);
	return;
}

ntcmp (n1, n2)
struct nts **n1;
struct nts **n2;
{
	return strcmp ((*n1)->t_name, (*n2)->t_name);
}
