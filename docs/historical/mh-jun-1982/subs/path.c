#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"

char *path(name, type)
	register char *name;
{
	static char *pwds;

	if(*name == '/' ||
	   (type == TFOLDER && *name != '.'))
		return name;
	if(!pwds)
		pwds = pwd();
	if(*name == '.' && name[1] == '/')
		name += 2;
	return concat(pwds, "/", name, 0);
}
