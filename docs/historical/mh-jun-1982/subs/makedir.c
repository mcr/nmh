#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>

makedir(dir)
	char *dir;
{
	register int pid, wpid;
	register char *c;
	int status;

	if((pid = fork()) == 0) {
		execl("/bin/mkdir", "mkdir", dir, 0);
		execl("/usr/bin/mkdir", "mkdir", dir, 0);
		fprintf(stderr, "Can't exec mkdir!!?\n");
		return(0);
	}
	if(pid == -1) {
		fprintf(stderr, "Can't fork\n");
		return(0);
	}
	while((wpid = wait(&status)) != pid && wpid != -1) ;
	if(status) {
		fprintf(stderr, "Bad exit status (%o) from mkdir.\n", status);
		return(0);
	}
	if((c = m_find("folder-protect")) == NULL)
		c = foldprot;
	VOID chmod(dir, atooi(c));
	return(1);
}
