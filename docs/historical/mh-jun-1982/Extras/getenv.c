#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include <stdio.h>
#include <pwd.h>

char *
getenv(key)
	char *key;
{
	register char *r;
	register struct passwd *p;

	if(strcmp(key, "USER") == 0 || strcmp(key, "HOME") == 0)
		if((p = getpwuid(getruid())) == NULL)
			return NULL;
		else
			return strcmp(key, "USER") == 0 ?
			       p->pw_name : p->pw_dir;
	return NULL;
}
