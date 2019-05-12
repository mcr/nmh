#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>
#include <strings.h>
#include "scansub.h"

long time();

int hdrflag;
int timeflag;
int numflag;
struct msgs *mp;

struct swit switches[] = {
	"all",         -3,      /* 0 */
	"ff",           0,      /* 1 */
	"noff",         0,      /* 2 */
	"header",       0,      /* 3 */
	"noheader",     0,      /* 4 */
	"help",         4,      /* 5 */
	"time",         0,      /* 6 */
	"notime",       0,      /* 7 */
	"numdate",      0,      /* 8 */
	"nonumdate",    0,      /* 9 */
	0,              0
};

extern  char _sobuf[];          /* MLW  standard out buffer */

/*ARGSUSED*/
main(argc, argv)
char *argv[];
{
	char *folder, *maildir, *msgs[100];
	register int msgnum;
	register char *cp, **ap;
	int msgp, ff;
	FILE *in;
	long now;
	char *arguments[50], **argp;

	invo_name = argv[0];
#ifndef _IOLBF
	setbuf(stdout, _sobuf);
#endif  _IOLBF
#ifdef NEWS
	m_news();
#endif
	ff = 0; msgp = 0; folder = 0;
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
			case -1:fprintf(stderr, "scan: -%s unknown\n", cp);
				goto leave;
							 /* -all */
			case 0: fprintf(stderr, "\"-all\" changed to \"all\"\n");
				goto leave;
			case 1: ff = 1;  continue;      /* -ff */
			case 2: ff = 0;  continue;      /* -noff */
			case 3: hdrflag = 1;  continue; /* -header */
			case 4: hdrflag = 0;  continue; /* -noheader */
			case 6: timeflag = 1;  continue; /* -time */
			case 7: timeflag = 0;  continue; /* -notime */
			case 8: numflag = 1;  continue; /* -numdate */
			case 9: numflag = 0;  continue; /* -nonumdate */
			case 5: help("scan [+folder]  [msgs] [switches]",
				     switches);
				goto leave;
			}
		if(*cp == '+') {
			if(folder) {
				fprintf(stderr, "Only one folder at a time.\n");
				goto leave;
			} else
				folder = path(cp+1, TFOLDER);
		} else
			msgs[msgp++] = cp;
	}
	if(!m_find("path")) free(path("./", TFOLDER));
	if(!folder)
		folder = m_getfolder();
	maildir = m_maildir(folder);
	if(chdir(maildir) < 0) {
		fprintf(stderr, "Can't chdir to: ");
		perror(maildir);
		goto leave;
	}
	if(!(mp = m_gmsg(folder))) {
		fprintf(stderr, "Can't read folder!?\n");
		goto leave;
	}
	if(mp->hghmsg == 0) {
		fprintf(stderr, "No messages in \"%s\".\n", folder);
		goto leave;
	}
	if(!msgp)
		msgs[msgp++] = "first-last";
	for(msgnum = 0; msgnum < msgp; msgnum++)
		if(!m_convert(msgs[msgnum]))
			goto leave;
	if(mp->numsel == 0) {
		fprintf(stderr, "scan: matzo balls.\n");         /* never get here */
		goto leave;
	}
	m_replace(pfolder,folder);
	for(msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)  {
		if(mp->msgstats[msgnum]&SELECTED) {
			if((in = fopen(cp = m_name(msgnum), "r")) == NULL)
				fprintf(stderr, "--Can't open %s\n", cp);
			else {
				if(hdrflag) {
					now = time((long *)0);
					cp = cdate(&now);
					cp[9] = ' '; cp[15] = 0;
printf("\
		       Folder %-32s%s\n\n", folder, cp);
				}
				VOID scan(in, msgnum, 0,
					  msgnum == mp->curmsg,
					  (timeflag ? DOTIME : 0)
					  | (numflag ? NUMDATE : 0), hdrflag);
				hdrflag = 0;
				VOID fclose(in);
				if(stdout->_cnt < 80)
					VOID fflush(stdout);
			}
		}
	}
	if(ff)
		putchar('\014');
leave:
	m_update();
	done