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

extern struct swit anoyes[];    /* Std no/yes gans array */
char   *malloc();

char defpath[128];

extern  char _sobuf[];          /* MLW  standard out buffer */

main(argc, argv)
char *argv[];
{
	register char *cp, *path;
	register struct node *np;
	int autof, exitstat;
	struct stat stbuf;
	char *geta();

	invo_name = argv[0];
	setbuf(stdout, _sobuf);
	autof = (argc == 2 && strcmp(argv[1], "-auto") == 0);
	exitstat = 1;                 /* Assume errors will occur */
	mypath = getenv("HOME");
/***    mypath = getpath(getruid());  /* to prevent recursion via m_getdefs */
	VOID copy(mh_prof, copy(mypath, defpath));
	if(stat(defpath, &stbuf) != -1) {
	    if(autof)
		printf("Install-defs invocation error!\n");
	    else
		printf("You already have an MH profile... use an editor \
to modify it.\n");
	    goto leave;

	}
	if(autof || gans("Do you want help? ", anoyes)) {
printf("\nPrior to using MH, it is necessary to have a file in your login\n");
printf("directory (%s) named %s which contains information\n",mypath,mh_prof+1);
printf("to direct certain MH operations.  The only item which is required\n");
printf("is the path to use for all MH folder operations. The suggested MH\n");
printf("path for you is %s/Mail...\n\n", mypath);
	}
	cp = concat(mypath, "/", "Mail", NULLCP);
	if(stat(cp, &stbuf) != -1) {
	    if((stbuf.st_mode&S_IFMT) == S_IFDIR) {
		cp = concat("You already have the standard MH directory \"",
			cp, "\".\nDo you want to use it for MH? ", NULLCP);
		if(gans(cp, anoyes))
		    path = "Mail";
		else
		    goto xyz;
	    }
	} else {
	    cp = concat("Do you want the standard MH path \"", mypath,
			 "/", "Mail\"? ", NULLCP);
	    if(gans(cp, anoyes))
		    path = "Mail";
	    else {
    xyz:        if(gans("Do you want a path below your login directory? ",
		    anoyes)) {
		    printf("What is the path ??  %s/", mypath);
		    path = geta();
		} else {
		    printf("What is the whole path??  /");
		    path = concat("/", geta(), NULLCP);
		}
	    }
	}
	VOID chdir(mypath);
	if(chdir(path) == -1) {
		cp = concat("\"", path, "\" doesn't exist; Create it? ", NULLCP);
		if(gans(cp, anoyes))
			if(makedir(path) == 0) {
				printf("Can't create it!\n");
				goto leave;
			}
	} else
		printf("[Using existing directory]\n");

	np = m_defs = (struct node *) malloc(sizeof *np);
	np->n_name = "Path";
	np->n_field = path;
	np->n_next = 0;
	m_replace(pfolder, defalt);
	exitstat = 0;

leave:
	m_update();
	done(exitstat);
}


char *geta()
{
	static char line[128];
	register char *cp;
	register int c;

	VOID fflush(stdout);
	cp = line;
	while((c = getchar()) != EOF) {
		if(c == '\n') {
			*cp = 0;
			return(line);
		}
		if(cp < &line[128])
			*cp++ = c;
	}
	done(1);
	/*NOTREACHED*/
}
