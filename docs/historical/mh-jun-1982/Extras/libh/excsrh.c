#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#
#include "../h/errors.h"

char *SRHLIST[] {	/* upper case!! */
	"",
	"%",
	"/bin/",
	"/usr/bin/",
	0
};

char **srhlist SRHLIST;

execvsrh(argv)
char **argv;
{
	return(excvsrh(argv[0], argv));
}

excvsrh(prog, argv)
char *prog, **argv;
{
	extern errno;
	register char **r1, *r2, *r3;
	static char command[50];

	for (r1 = srhlist; r2 = *r1++; ) {
		r3 = command;
		if(*r2 == '%') {
			r2 = getpath(getruid()&0377);
			while(*r3++ = *r2++);
			--r3;
			r2 = "/bin/";
		}
		while (*r3++ = *r2++);
		--r3;
		r2 = prog;
		while (*r3++ = *r2++);
		if (execvsh(command, argv) != 0) break;
	}
	errtype(prog, errno==E2BIG?"arg list too long":"not found");
	return(-1);
}
execlsrh(arg1,arg2,arg3)
char *arg1,*arg2,*arg3;
{
	return(excvsrh(arg1, &arg1));
}

exclsrh(prog, arg1, arg2)
char *prog, *arg1, *arg2;
{
	return(excvsrh(prog, &arg1));
}
