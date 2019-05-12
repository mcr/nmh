#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>
#include <strings.h>

int vecp;
int     header = 1;
char *vec[MAXARGS];
struct msgs *mp;
/*  The minimum match numbers below are all at least 2 as
/*  a kludge to avoid conflict between switches intended for
/*  "show" and those that it passes on to pr, mhl, c, ...
/**/
struct swit switches[] = {
	"all",         -3,      /* 0 */
	"draft",        2,      /* 1 */
	"header",       2,      /* 2 */
	"noheader",     2,      /* 3 */
	"format",       2,      /* 4 */
	"noformat",     2,      /* 5 */
	"pr",           2,      /* 6 */
	"nopr",         2,      /* 7 */
	"help",         4,      /* 8 */
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
	int msgp, drft, pr, format;
	char *arguments[50], **argp;

	invo_name = argv[0];
	setbuf(stdout, _sobuf);
#ifdef NEWS
	m_news();
#endif
	folder = (char *) 0;
	pr = msgp = 0;
	format = 1;
	vecp = 1;
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
			case -1:vec[vecp++] = --cp;  continue;
							     /* -all   */
			case 0: fprintf(stderr, "\"-all\" changed to \"all\"\n");
				goto leave;
			case 1: drft = 1;  continue;         /* -draft */
			case 2: header = 1; continue;        /* -header */
			case 3: header = 0; continue;        /* -noheader */
			case 4: format = 1;  continue;       /* -format */
			case 5: format = 0;  continue;       /* -noformat */
			case 6: pr = 1;  continue;           /* -pr    */
			case 7: pr = 0;  vecp = 1;  continue;/* -nopr  */
			case 8:                              /* -help  */
  help("show [+folder]  [msgs] [switches] [switches for \"type\" or \"pr\" ]",
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
	if(drft)
		maildir = m_maildir("");
	else {
		if(!msgp)
			msgs[msgp++] = "cur";
		if(!folder)
			folder = m_getfolder();
		maildir = m_maildir(folder);
	}
	if(chdir(maildir) < 0) {
		fprintf(stderr, "Can't chdir to: ");
		perror(maildir);
		goto leave;
	}
	if(drft) {
		vec[vecp++] = draft;
		goto doit;
	}
	if(!(mp = m_gmsg(folder))) {
		fprintf(stderr, "Can't read folder!?\n");
		goto leave;
	}
	if(mp->hghmsg == 0) {
		fprintf(stderr, "No messages in \"%s\".\n", folder);
		goto leave;
	}
	if(msgp)
		for(msgnum = 0; msgnum < msgp; msgnum++)
			if(!m_convert(msgs[msgnum]))
				goto leave;
	if(mp->numsel == 0) {
		fprintf(stderr, "show: potato pancakes.\n");     /* never get here */
		goto leave;
	}
	if(mp->numsel > MAXARGS-2) {
  fprintf(stderr, "show: more than %d messages for show-exec\n", MAXARGS-2);
		goto leave;
	}
	for(msgnum= mp->lowsel; msgnum<= mp->hghsel; msgnum++)
		if(mp->msgstats[msgnum]&SELECTED)
			vec[vecp++] = getcpy(m_name(msgnum));
	m_replace(pfolder, folder);
	if(mp->hghsel != mp->curmsg)
		m_setcur(mp->hghsel);
	if(vecp == 2 && header) {
		printf("(Message %s:%s)\n", folder, vec[1]);
	}
doit:
	VOID fflush(stdout);
	vec[vecp] = 0;
	{
		register char *proc;
		if(pr)
			proc = prproc;
		else if(format) {
			extern char *r1bindex();
			putenv("mhfolder", folder);
			if (!strcmp (r1bindex(showproc, '/'), "mhl")) {
				mhl(vecp, vec);
				m_update();
				done(0);
			}
			proc = showproc;
		} else {
			proc = "/bin/cat";
			/* THIS IS INEFFICIENT */
			/* what we really should do in this case is
			/* copy it out ourself to save the extra exec */
		}
		m_update();
		vec[0] = r1bindex(proc, '/');
		execv(proc, vec);
		perror(proc);
	}
	done(0);
 leave:
	m_update();
	done(0);
}

#define switches mhlswitches
#define INCLUDED_BY_SHOW
#define main(a,b) mhl(a,b)

#include 