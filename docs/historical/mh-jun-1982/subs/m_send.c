#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>

m_send(arg, file)
char **arg, *file;
{
	char *vec[15];
	int ivec;

	ivec = 0;
	vec[ivec++] = "send";
	vec[ivec++] = file;
	if(**arg == 'v')
		vec[ivec++] = "-verbose";
	else if(*arg) {
		do
			vec[ivec++] = *arg++;
		while(*arg);
	}
	vec[ivec++] = 0;
	m_update();
	VOID fflush(stdout);
	execv(sendproc, vec);
	fprintf(stderr, "Can't exec %s.\n", sendproc);
	return(0);

}
