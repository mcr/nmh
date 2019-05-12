#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>
#include <strings.h>
char *calloc();

int     vecp;
char    **vec;
struct msgs *mp;

struct swit switches[] = {
	"all",         -3,      /* 0 */
	"help",         4,      /* 1 */
	0,              0
};


/*ARGSUSED*/
main(argc, argv)
char *argv[];
{
	char *folder, *maildir, *msgs[100], buf[32];
	register int msgnum;
	register char *cp, *sp;
	int msgp;
	char **ap;
	char *arguments[50], **argp;

	invo_name = argv[0];
#ifdef NEWS
	m_news();
#endif
	folder = 0; msgp = 0;
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
			case -1:fprintf(stderr, "rmm: -%s unknown\n", cp);
				goto leave;
							 /* -all */
			case 0: fprintf(stderr, "\"-all\" changed to \"all\"\n");
				goto leave;
							/* -help */
			case 1: help("rmm [+folder]  [msgs] [switches]",
				     switches);
				goto leave;
			}
		if(*cp == '+')  {
			if(folder) {
				fprintf(stderr, "Only one folder at a time.\n");
				goto leave;
			} else
				folder = path(cp+1, TFOLDER);
		} else
			msgs[msgp++] = cp;
	}
	if(!m_find("path")) free(path("./", TFOLDER));
#ifdef COMMENT
	if(!msgp)
		msgs[msgp++] = "cur";
#endif
	if(!folder)
		folder = m_getfolder();
	maildir = m_maildir(folder);
	if(chdir(maildir) < 0) {
		fprintf(stderr, "Can't chdir to: ");
		perror(maildir);
		goto leave;
	}
	if(!msgp) {
		if((msgnum = m_getcur(folder)) == 0) {
			fprintf(stderr, "%s: No current message.\n", folder);
			goto leave;
		}
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
	for(msgnum = 0; msgnum < msgp; msgnum++)
		if(!m_convert(msgs[msgnum]))
			goto leave;
	if(mp->numsel == 0) {
		fprintf(stderr, "rmm: lasagne 'n sausage\n");     /* never get here */
		goto leave;
	}
doit:
	m_replace(pfolder, folder);
	if((cp = m_find("delete-prog")) == NULL) {
		if(!msgp)
			goto lp1;
		for(msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
			if(mp->msgstats[msgnum] & SELECTED) {
		     lp1:       sp = getcpy(m_name(msgnum));
				cp = copy(sp, buf);
				cp[1] = 0;
				do
					*cp = cp[-1];
				while(--cp >= buf && *cp != '/');
				*++cp = ',';            /* New backup convention */
				VOID unlink(buf);
				if(link(sp, buf) == -1 || unlink(sp) == -1)
					fprintf(stderr, "Can't rename %s to %s.\n", sp, buf);
				if(!msgp)
					goto leave;
			}
	} else {
		if(!msgp) {
			vec = (char **) calloc(3, sizeof *vec);
			vecp = 1;
			vec[vecp++] = getcpy(m_name(msgnum));
			goto lp2;
		}
		if(mp->numsel > MAXARGS-2) {
  fprintf(stderr, "rmm: more than %d messages for deletion-prog\n",MAXARGS-2);
			goto leave;
		}
		vec = (char **) calloc(MAXARGS +2, sizeof *vec);
		vecp = 1;
		for(msgnum= mp->lowsel; msgnum<= mp->hghsel; msgnum++)
			if(mp->msgstats[msgnum]&SELECTED)
				vec[vecp++] = getcpy(m_name(msgnum));
	lp2:    vec[vecp] = 0;
		vec[0] = r1bindex(cp, '/');
		m_update();
		VOID fflush(stdout);
		execv(cp, vec);
		fprintf(stderr, "Can't exec deletion prog--");
		perror(cp);
	}
leave:
	m_update();
	done(0);
}
