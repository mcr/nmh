#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include "../folder.h"
#include <stdio.h>

#define NFOLD 20                /* Allow 20 folder specs */
char *calloc();

/* file [-src folder] [msgs] +folder [+folder ...]
 *
 * moves messages from src folder (or current) to other one(s).
 *
 *  all = 1-999 (MAXFOLDER) for a message sequence
 *  -preserve says preserve msg numbers
 *  -link says don't delete old msg
 */

extern struct swit anoyes[];    /* Std no/yes gans array */

int vecp, foldp, prsrvf;
char **vec, maildir[128], *folder;
struct msgs *mp;

struct st_fold folders[NFOLD];

char   *files[NFOLD + 1];       /* Vec of files to process--starts at 1! */
int     filec = 1;

struct swit switches[] = {
	"all",           -3,      /* 0 */
	"link",           0,      /* 1 */
	"nolink",         0,      /* 2 */
	"preserve",       0,      /* 3 */
	"nopreserve",     0,      /* 4 */
	"src +folder",    0,      /* 5 */
	"file",           0,      /* 6 */
	"help",           4,      /* 7 */
	0,                0
};

/*ARGSUSED*/
main(argc, argv)
char *argv[];
{
	register int i, msgnum;
	register char *cp;
	char *msgs[128];
	int msgp, linkf;
	char **ap;
	char *arguments[50], **argp;

	invo_name = argv[0];
#ifdef NEWS
	m_news();
#endif
	folder = 0; msgp = 0; linkf = 0;
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
			case -1:fprintf(stderr, "file: -%s unknown\n", cp);
				goto leave;
						       /* -all */
			case 0: fprintf(stderr, "\"-all\" changed to \"all\"\n");
				goto leave;
			case 1: linkf = 1;  continue;  /* -link */
			case 2: linkf = 0;  continue;  /* -nolink */
			case 3: prsrvf = 1;  continue; /* -preserve */
			case 4: prsrvf = 0;  continue; /* -nopreserve */
			case 5: if(folder) {           /* -src */
					fprintf(stderr, "Only one src folder.\n");
					goto leave;
				}
				if(!(folder = *argp++) || *folder == '-') {
missing:        fprintf(stderr, "file: Missing argument for %s switch\n", argp[-2]);
					goto leave;
				}
				if(*folder == '+')
					folder++;
				folder = path(folder, TFOLDER);
				continue;
			case 6:
				if(filec >= NFOLD) {
					fprintf(stderr, "Too many src files.\n");
					goto leave;
				}
				if(!(cp = *argp++) || *cp == '-')
					goto missing;
				files[filec++] = path(cp, TFILE);
				continue;

							/* -help */
			case 7: help("file   [msgs] [switches]  +folder ...",
				     switches);
				goto leave;
			}
		if(*cp == '+')  {
			if(foldp < NFOLD)
				folders[foldp++].f_name = path(++cp, TFOLDER);
			else {
				fprintf(stderr, "Only %d folders allowed.\n", NFOLD);
				goto leave;
			}
		} else
			msgs[msgp++] = cp;
	}
	if(!m_find("path")) free(path("./", TFOLDER));
	if(!foldp) {
		fprintf(stderr, "No folder specified.\n");
fprintf(stderr, "Usage: file [-src folder] [msg ...] [switches] +folder [+folder]\n");
		goto leave;
	}
	if(filec > 1) {
		if(msgp) {
			fprintf(stderr, "File: Can't mix files and messages.\n");
			goto leave;
		}
		if(opnfolds())
			goto leave;
		for(i = 1; i < filec; i++) {
			if(m_file(folder, files[i], folders, foldp, prsrvf, 0))
				goto leave;
		}
		if(!linkf) {
			if((cp = m_find("delete-prog")) != NULL) {
				files[0] = r1bindex(cp, '/');
				execvp(cp, files);
				fprintf(stderr, "Can't exec deletion-prog--");
				perror(cp);
			} else for(i = 1; i < filec; i++) {
				if(unlink(files[i]) == -1) {
					fprintf(stderr, "Can't unlink ");
					perror(files[i]);
				}
			}
		}
		goto leave;
	}
	if(!msgp)
		msgs[msgp++] = "cur";
	if(!folder)
		folder = m_getfolder();
	VOID copy(m_maildir(folder), maildir);
	if(chdir(maildir) < 0) {
		fprintf(stderr, "Can't chdir to: ");
		perror(maildir);
		goto leave;
	}
	if(!(mp = m_gmsg(folder))) {
		fprintf(stderr, "Can't read folder %s!?\n",folder);
		goto leave;
	}
	if(mp->hghmsg == 0) {
		fprintf(stderr, "No messages in \"%s\".\n", folder);
		goto leave;
	}
	for(msgnum = 0; msgnum < msgp; msgnum++)
		if(!m_convert((cp = msgs[msgnum])))
			goto leave;
	if(mp->numsel == 0) {
		fprintf(stderr, "file:  ham 'n cheese\n");       /* never get here */
		goto leave;
	}
	m_replace(pfolder, folder);
	if(mp->hghsel != mp->curmsg && ((mp->numsel != mp->nummsg) || linkf))
		m_setcur(mp->hghsel);
	if(opnfolds())
		goto leave;
	for(msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
		if(mp->msgstats[msgnum] & SELECTED)
			if(m_file(folder, cp = getcpy(m_name(msgnum)),
				  folders, foldp, prsrvf, 0))
				goto leave;
			else
				cndfree(cp);
	if(!linkf) {
		if((cp = m_find("delete-prog")) != NULL) {
			if(mp->numsel > MAXARGS-2) {
	  fprintf(stderr, "file: more than %d messages for deletion-prog\n",MAXARGS-2);
				printf("[messages not unlinked]\n");
				goto leave;
			}
			vecp = 1;
			vec = (char **) calloc(MAXARGS + 2, sizeof *vec);
			for(msgnum= mp->lowsel; msgnum<= mp->hghsel; msgnum++)
				if(mp->msgstats[msgnum]&SELECTED)
					vec[vecp++] = getcpy(m_name(msgnum));
			vec[vecp] = 0;
			m_update();
			VOID fflush(stdout);
			vec[0] = r1bindex(cp, '/');
			execv(vec[0], vec);
			fprintf(stderr, "Can't exec deletion-prog--");
			perror(cp);
		} else {
			for(msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
				if(mp->msgstats[msgnum] & SELECTED)
					if(unlink(cp = m_name(msgnum))== -1) {
						fprintf(stderr, "Can't unlink %s:",folder);
						perror(cp);
					}
		}
	}
leave:
	m_update();
	done(0);
}


opnfolds()
{
	register int i;
	register char *cp;
	char nmaildir[128];

	for(i = 0; i < foldp; i++) {
		VOID copy(m_maildir(folders[i].f_name), nmaildir);
		if(access(nmaildir, 5) < 0) {
			cp = concat("Create folder \"", nmaildir, "\"? ", 0);
			if(!gans(cp, anoyes))
				goto bad;
			free(cp);
			if(!makedir(nmaildir)) {
				fprintf(stderr, "Can't create folder.\n");
				goto bad;
			}
		}
		if(chdir(nmaildir) < 0) {
			fprintf(stderr, "Can't chdir to: ");
			perror(nmaildir);
			goto bad;
		}
		if(!(folders[i].f_mp = m_gmsg(folders[i].f_name))) {
			fprintf(stderr, "Can't read folder %s\n", folders[i].f_name);
			goto bad;
		}
	}
	VOID chdir(maildir);    /* return to src folder */
	return(0);
bad:
	retur