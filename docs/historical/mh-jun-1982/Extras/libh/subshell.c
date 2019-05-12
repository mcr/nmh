#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../h/errors.h"

subshell(str)
char *str;
{
	register s2, s3;
	char *a2, *a3;
	register pid;
	char st[2];
	/* if str is non null, exec it as a command to the shell */
	/* and return immediately */
	/* if str is the null string, simply spawn a sub-shell */

	if (str == 0 || *str == 0) {
		a2 = 0;
		a3 = 0;
	} else {
		a2 = "-c";
		a3 = str;
	}
	pid = fork();
	if (pid == 0) {
		execl("/bin/sh", "-", a2, a3, 0);
		error("no shell!");
	}
	if (pid == -1) {
		type(2, errno == EAGAIN? "system": "your");
		type(2, " process limit reached\n");
		return(-1);
	}
	s2 = signal(2,1);
	s3 = signal(3,1);
	while(pid != wait(&st));
	signal(2,s2);
	signal(3,s3);
	return(st[1]);
}
