#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include "../folder.h"
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
char *malloc();

#define NFOLD 20                /* Allow 20 folder specs */

extern struct swit anoyes[];    /* Std no/yes gans array */

/*
 * pick [-src folder] [msgs] search [-scan] [-show] [file-op]
 *
 *  search =  -from        \
 *            -to           \
 *            -cc            \
 *            -subject        \  pattern
 *            -sub            /
 *            -date          /
 *            -search       /
 *            --component  /
 *
 * file-op =  -file [-preserve] [-link] +folder ...
 *            -keep [-stay] [+folder ...]
 */

int nvecp, foldp;
char **nvec;
struct msgs *mp;
char grep[256], *grepp, *folder, maildir[128];
int showf, scanfl, filef, keepf, linkf, noteold, prsrvf, stayf;
int grep_lowsel  = 5000,
    grep_hghsel  = 0;
char *delprog;

struct st_fold folders[NFOLD];

struct swit switches[] = {
	"cc  pattern",                  0,      /*  0 */
	"date  pattern",                0,      /*  1 */
	"from  pattern",                0,      /*  2 */
	"search  pattern",              0,      /*  3 */
	"subject  pattern",             0,      /*  4 */
	"to  pattern",                  0,      /*  5 */
	"-othercomponent  pattern",    15,      /*  6 */
	"all",                         -3,      /*  7 */
	"file  +folder ...",            0,      /*  8 */
	"nofile",                       0,      /*  9 */
	"keep [+folder ...]",           0,      /* 10 */
	"nokeep",                       0,      /* 11 */
	"link",                         0,      /* 12 */
	"nolink",                       0,      /* 13 */
	"preserve",                     0,      /* 14 */
	"nopreserve",                   0,      /* 15 */
	"scan",                         0,      /* 16 */
	"noscan",                       0,      /* 17 */
	"show",                         0,      /* 18 */
	"noshow",                       0,      /* 19 */
	"src  +folder",                 0,      /* 20 */
	"stay",                         0,      /* 21 */
	"nostay",                       0,      /* 22 */
	"help",                         4,      /* 23 */
	0,                              0
};

/*ARGSUSED*/
main(argc, argv)
char *argv[];
{
	char *msgs[128], buf[128];
	register int msgnum;
	register char *cp;
	int msgp, i;
	char **ap;
	char *arguments[50], **argp, **arrp;

	invo_name = argv[0];
#ifdef NEWS
	m_news();
#endif
	nvecp = 1;
	msgp = 0;
	grepp = grep;
	cp = r1bindex(argv[0], '/');
	if((cp = m_find(cp)) != NULL) {
		ap = brkstring(cp = getcpy(cp), " ", "\n");
		ap = copyip(ap, arguments);
	} else
		ap = arguments;
	VOID copyip(argv+1, ap);
	argp = arguments;
	while(cp = *argp++) {
		if(*cp == '-')  {
			if(*++cp == '-') {              /* --component */
		    toomany:    if(grepp != grep) {
				    fprintf(stderr, "Only one search string.\n");
				    goto leave;
				}
				grepp = copy("^", grepp);
				grepp = copy(++cp, grepp);
				grepp = copy(":.*", grepp);
				goto pattern;
			}
			switch(i = smatch(cp, switches)) {
			case -2:ambigsw(cp, switches);  /* ambiguous */
				goto leave;
							/* unknown */
			case -1:fprintf(stderr, "pick: -%s unknown\n", cp);
				goto leave;
							/* -component */
			case 0:  case 1:  case 2:  case 4:  case 5:
				if(grepp != grep)
					goto toomany;
				grepp = copy("^", grepp);
				arrp = brkstring(switches[i].sw, " ", NULLCP);
				grepp = copy(*arrp, grepp);
				grepp = copy(":.*", grepp);
			case 3:                         /* -search */
		       pattern: grepp = copy(*argp++, grepp);
				continue;
			case 6: fprintf(stderr, "pick:  can't get here\n");
				goto leave;
						       /* -all */
			case 7: fprintf(stderr, "\"-all\" changed to \"all\"\n");
				goto leave;
			case 8: filef = 1;  continue;   /* -file */
			case 9: filef = 0;  continue;   /* -nofile */
			case 10:keepf = 1;  continue;   /* -keep */
			case 11:keepf = 0;  continue;   /* -nokeep */
			case 12:linkf = 1;  continue;   /* -link */
			case 13:linkf = 0;  continue;   /* -nolink */
			case 14:prsrvf = 1;  continue;  /* -preserve */
			case 15:prsrvf = 0;  continue;  /* -nopreserve */
			case 16:scanfl = 1;  continue;   /* -scan */
			case 17:scanfl = 0;  continue;   /* -noscan */
			case 18:showf = 1;  continue;   /* -show */
			case 19:showf = 0;  continue;   /* -noshow */
			case 21:stayf = 1;  continue;   /* -stay */
			case 22:stayf = 0;  continue;   /* -nostay */
			case 20:if(folder) {            /* -src */
					fprintf(stderr, "Only one src folder.\n");
					goto leave;
				}
				if(!(folder = *argp++) || *folder == '-') {
		fprintf(stderr, "pick: Missing argument for %s switch\n", argp[-2]);
					 goto leave;
				}
				if(*folder == '+')
					folder++;
				folder = path(folder, TFOLDER);
				continue;
							/* -help */
			case 23:help("pick   [msgs] [switches]", switches);
				goto leave;
			}
		} else if(*cp == '+') {
			if(foldp < NFOLD)
				folders[foldp++].f_name = path(cp+1, TFOLDER);
			else {
				fprintf(stderr, "Only %d folders allowed.\n", NFOLD);
				goto leave;
			}
		} else
			msgs[msgp++] = cp;
	}
	if(!m_find("path")) free(path("./", TFOLDER));
	if(grepp == grep) {
		fprintf(stderr, "No search pattern specified.\n");
		goto leave;
	}
	if(filef && keepf) {
		fprintf(stderr, "-file and -keep don't go together.\n");
		goto leave;
	}
	if(!scanfl && !showf && !filef)
		keepf++;                /* The default is -keep         */
	if(keepf) {
		prsrvf++;               /* -keep forces -preserve       */
		linkf++;                /*   and -link                  */
	}
	if(!folder)
		folder = m_getfolder(); /* use cur folder if no -src    */
	VOID copy(m_maildir(folder), maildir);
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
	if(!foldp) {                    /* if no +folder given...       */
		if(filef) {             /* -file requires one           */
			fprintf(stderr, "-file requires at least one folder arg.\n");
			goto leave;
		}
		if(keepf) {             /* use default selection-list name */
			VOID copy(listname, copy("/", copy(folder, buf)));
			folders[foldp++].f_name = getcpy(buf);
			noteold++;      /* tell user if existing folder */
		}
	} else if(keepf) {              /* make folders sub-folders     */
		for(msgnum = 0; msgnum < foldp; msgnum++)
			if(*(cp = folders[msgnum].f_name) != '.' &&
			   *cp != '/') {
				VOID copy(cp, copy("/", copy(folder, buf)));
				folders[msgnum].f_name = getcpy(buf);
			}
		noteold++;
	}
	if(!msgp)
		msgs[msgp++] = "first-last";
	for(msgnum = 0; msgnum < msgp; msgnum++)
		if(!m_convert(msgs[msgnum]))
			goto leave;
	if(mp->numsel == 0) {
		fprintf(stderr, "pick:  Peanut butter 'n jelly\n");/* never get here */
		goto leave;
	}
	if(!compile(grep)) {
		fprintf(stderr, "Pattern Error.\n");
		goto leave;
	}
	for(msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
		if(mp->msgstats[msgnum]&SELECTED)
			grepfn(msgnum);
	if(mp->numsel == 0) {
		fprintf(stderr, "No messages match specification.\n");
		goto leave;
	}
	mp->lowsel = grep_lowsel;
	mp->hghsel = grep_hghsel;
						      /* all the exec's */
	if((((delprog = m_find("delete-prog")) != NULL) &&
	    ((filef || keepf) && !linkf)) ||
	      scanfl || showf) {
		if(mp->numsel > MAXARGS-2) {
			fprintf(stderr, "pick: more than %d messages for %s exec\n",
				MAXARGS-2,
				scanfl ? "scan" : showf ? "show" : delprog);
			goto leave;
		}
		nvec = (char **) malloc(MAXARGS * sizeof nvec[0]);
		for(msgnum= mp->lowsel; msgnum<= mp->hghsel; msgnum++)
			if(mp->msgstats[msgnum]&SELECTED)
				nvec[nvecp++] = getcpy(m_name(msgnum));
		nvec[nvecp] = 0;
	}
	if(keepf || filef)
		if(opnfolds())
			goto leave;
	if(!noteold || foldp > 1)
		m_replace(pfolder, folder);
	if(scanfl)
		scanfn(showf|filef|keepf);
	else {
		printf("%d hit%s.\n", mp->numsel, mp->numsel == 1 ? "" : "s");
		VOID fflush(stdout);
	}
	if(showf)
		showfn(filef|keepf);
	if(!(filef|keepf))
		goto leave;
	for(msgnum= mp->lowsel; msgnum<= mp->hghsel; msgnum++)
		if(mp->msgstats[msgnum]&SELECTED)
			if(m_file(folder, cp = getcpy(m_name(msgnum)),
				  folders, foldp, prsrvf, 1))
				goto leave;
			else
				free (cp);
	if(!linkf)
		remove();
	if(noteold) {
		struct st_fold *fptr;
		if(!stayf && foldp == 1) {
			m_replace(pfolder, cp = folders[0].f_name);
			printf("[+%s now current]\n", cp); VOID fflush(stdout);
		}
		for(fptr = folders; fptr < &folders[foldp]; fptr++)
			if(!fptr->f_reused) {
				VOID chdir(m_maildir(fptr->f_name));
				m_setcur(fptr->f_mp->curmsg);
			}
	}
 leave:
	m_update();
}


grepfn(msg)
{
	if(execute(m_name(msg))) {                  /* a match */
		if(msg < grep_lowsel)
			grep_lowsel = msg;
		if(msg > grep_hghsel)
			grep_hghsel = msg;
	} else {
		mp->msgstats[msg] &= ~SELECTED;     /* clear SELECTED bit */
		mp->numsel--;
	}
}


opnfolds()
{
	register int i;
	register char *cp, *ap;
	char nmaildir[128];
	struct stat stbuf;

	for(i = 0; i < foldp; i++) {
		VOID copy(m_maildir(cp = folders[i].f_name), nmaildir);
		if(stat(nmaildir, &stbuf) < 0) {
			if(!noteold) {
				ap = concat("Create folder \"",
					nmaildir, "\"? ", 0);
				if(!gans(ap, anoyes))
					return(1);
			}
			if(!makedir(nmaildir)) {
				fprintf(stderr, "Can't create folder.\n");
				return(1);
			}
		} else if(noteold) {
			printf("[Folder %s being re-used.]\n", cp);
			VOID fflush(stdout);
			folders[i].f_reused++; /* Don't change cur in old fold */
		}
		if(chdir(nmaildir) < 0) {
			fprintf(stderr, "Can't chdir to: ");
			perror(nmaildir);
			return(1);
		}
		if(!(folders[i].f_mp = m_gmsg(folders[i].f_name))) {
			fprintf(stderr, "Can't read folder %s\n", folders[i].f_name);
			return(1);
		}
		folders[i].f_mp->curmsg = 0;
	}
	VOID chdir(maildir);    /* return to src folder */
	return(0);
}


scanfn(forkf)
{
	register int pid;

	nvec[0] = r1bindex(scanproc, '/');
	if(forkf && (pid = fork())) {
		if(pid == -1) {
			fprintf(stderr, "No forks!\n");
			done(1);
		}
		while(wait((int *)NULL) != pid) ;
	} else {
		m_update();
		VOID fflush(stdout);
		execv(scanproc, nvec);
		perror(scanproc);
		done(1);
	}
}


showfn(forkf)
{
	register int pid;
	int (*sint)(), (*sqit)();

	nvec[0] = r1bindex(showproc, '/');
	if(forkf) {
		sint = signal(SIGINT, SIG_IGN);
		sqit = signal(SIGQUIT, SIG_IGN);
	}
	if(forkf && (pid = fork())) {
		if(pid == -1) {
			fprintf(stderr, "No forks!\n");
			done(1);
		}
		while(wait((int *)NULL) != pid) ;
		VOID signal(SIGINT, sint);
		VOID signal(SIGQUIT, sqit);
	} else {
		m_update();
		VOID fflush(stdout);
		putenv("mhfolder", folder);
		execv(showproc, nvec);
		perror(showproc);
		done(1);
	}
}


remove()
{
	register int i;
	register char *cp;

	if(delprog != NULL) {
		nvec[0] = r1bindex(delprog, '/');
		m_update();
		VOID fflush(stdout);
		execv(delprog, nvec);
		fprintf(stderr, "Can't exec deletion-prog--");
		perror(delprog);
	} else {
		for(i= mp->lowsel; i<= mp->hghsel; i++)
			if(mp->msgstats[i]&SELECTED)
				if(unlink(cp = m_name(i)) == -1) {
					fprintf(stderr, "Can't unlink %s:",folder);
					perror(cp);
			