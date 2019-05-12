#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <signal.h>
#include <stdio.h>

showfile(file)
char *file;
{
	int pid, wpid, (*intr)(), status;
	char *vec[4];

	intr = signal(SIGINT, SIG_IGN);
	m_update();
	VOID fflush(stdout);
	if((pid = fork()) == 0) {
		vec[0] = r1bindex(lproc, '/');
		vec[1] = file;
		vec[2] = 0;
		VOID signal(SIGINT, intr);
		execv(lproc, vec);
		fprintf(stderr, "Can't exec ");
		perror(lproc);
		goto badleave;
	} else if(pid == -1) {
		fprintf(stderr, "No forks!\n");
		goto badleave;
	} else
		while((wpid = wait(&status)) != -1 && wpid != pid) ;
	VOID signal(SIGINT, intr);
	if(status & 0377)
		goto badleave;
	return(0);

 badleave:
	VOID fflush(stdout);
	return(1);

}

