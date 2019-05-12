#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>

extern char *sprintf();
extern char *strcpy();

char *mypath;

char *m_maildir(folder)
char *folder;
{
	register char *fold, *pth, *cp;
	static char mailfold[128];

	m_getdefs();
	if(!(fold = folder))
		fold = m_getfolder();
	if(*fold == '/' || *fold == '.')
		return(fold);
	cp = mailfold;
	if((pth = m_find("path")) != NULL && *pth) {
		if(*pth != '/') {
			VOID sprintf(cp, "%s/", mypath);
			cp += strlen(cp);
		}
		cp = copy(pth, cp);
		if(cp[-1] != '/')
			*cp++ = '/';
	} else
		cp = copy(path("./", TFOLDER), cp);
	strcpy(cp, fold);
	return(mailf