#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>
#include <strings.h>

extern struct swit anoyes[];    /* Std no/yes gans array */

int     subf;

struct dirent {
	short   inum;
	char    name[14];
	int     pad;
};

struct swit switches[] = {
	"help",         4,      /* 0 */
	0,              0
};


/*ARGSUSED*/
main(argc, argv)
char *argv[];
{
	register char *cp, **ap;
	char *folder, buf[128];
	int def_fold = 0;
	char *arguments[50], **argp;

	invo_name = argv[0];
#ifdef NEWS
	m_news();
#endif
	folder = 0;
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
			case -1:fprintf(stderr, "rmf: -%s unknown\n", cp);
				goto leave;
							/* -help */
			case 0: help("rmf [+folder]  [switches]", switches);
				goto leave;
			}
		if(*cp == '+')
			if(folder) {
				fprintf(stderr, "Only one folder at a time.\n");
				goto leave;
			} else
				folder = path(cp+1, TFOLDER);
		else {
			fprintf(stderr, "Usage: rmf [+folder]\n");
			goto leave;
		}
	}
	if(!m_find("path")) free(path("./", TFOLDER));
	if(!folder) {
		folder = m_getfolder();
		def_fold++;
	}
	subf = !((!index(folder, '/')) | (*folder == '/') | (*folder == '.'));
	if(def_fold && !subf) {
		cp = concat("Remove folder \"", folder, "\" ?? ", NULLCP);
		if(!gans(cp, anoyes))
			goto leave;
		free(cp);
	}
	if(rmfold(folder))
		goto leave;
	if(subf) {                      /* make parent "current" */
		cp = copy(folder, buf);
		while(cp > buf && *cp != '/') --cp;
		if(cp > buf) {
			*cp = 0;
			if(strcmp(m_find(pfolder), buf) != 0) {
				printf("[+%s now current]\n", buf);
				m_replace(pfolder, buf);
			}
		}
	}
 leave:
	m_update();
	done(0);
}

struct dirent ent;

rmfold(fold)
char *fold;
{
	register char *maildir;
	int i, leftover, cd;

	leftover = 0;
	if(!subf && strcmp(m_find(pfolder), fold) == 0) /* make default "current"*/
		if(strcmp(m_find(pfolder), defalt) != 0) {
			printf("[+%s now current]\n", defalt);
			VOID fflush(stdout);                    /*??*/
			m_replace(pfolder, defalt);
		}
	maildir = m_maildir(fold);
	if((cd = chdir(maildir)) < 0)
		goto funnyfold;
	if(access(".", 2) == -1) {
 funnyfold:     if(!m_delete(concat("cur-", fold, NULLCP)))
			printf("[Folder %s de-referenced]\n", fold);
		else
			fprintf(stderr, "You have no profile entry for the %s folder %s\n",
			  cd < 0 ? "unreadable" : "read-only", fold);
		return(1);
	}
	i = open(".", 0);
	ent.pad = 0;
	while(read(i, (char *)&ent.inum, sizeof ent.name + sizeof ent.inum))
		if(ent.inum)
		    if((ent.name[0] >= '0' && ent.name[0] <= '9') ||
			ent.name[0] == ',' ||
			(ent.name[0] == '.' && ent.name[1] && ent.name[1] != '.') ||
			strcmp(ent.name, "cur") == 0 ||
			strcmp(ent.name, "@") == 0) {
			    if(unlink(ent.name) == -1) {
				fprintf(stderr, "Can't unlink %s:%s\n", fold,ent.name);
				leftover++;
			    }
		    } else if(strcmp(ent.name,".") != 0 &&
			      strcmp(ent.name,"..") != 0) {
			fprintf(stderr, "File \"%s/%s\" not deleted!\n",
				fold, ent.name);
			leftover++;
		    }
	VOID close(i);
	VOID chdir("..");       /* Move out of dir to be deleted */
	if(!leftover && removedir(maildir))
		return(0);
	else
		fprintf(stderr, "Folder %s not removed!\n", fold);
	return(1);
}


removedir(dir)
	char *dir;
{
	register int pid, wpid;
	int status;

	if((pid = fork()) == 0) {
		m_update();
		VOID fflush(stdout);
		execl("/bin/rmdir", "rmdir", dir, 0);
		execl("/usr/bin/rmdir", "rmdir", dir, 0);
		fprintf(stderr, "Can't exec rmdir!!?\n");
		return(0);
	}
	if(pid == -1) {
		fprintf(stderr, "Can't fork\n");
		return(0);
	}
	while((wpid = wait(&status)) != pid && wpid != -1) ;
	if(status) {
		fprintf(stderr, "Bad exit status (%o) from rmdir.\n", status);
	    /*  return(0);   */
	}
	retur