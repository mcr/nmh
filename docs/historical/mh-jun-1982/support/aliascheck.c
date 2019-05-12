#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/*
 * aliascheck(name) will return an indication of whether name is
 *  a valid alias from AliasFile.  The return values are:
 *
 *      1 -> yes
 *      0 -> no
 *     -1 -> an error in the alias file
 */

#include <stdio.h>
#include <ctype.h>
char    *AliasFile =    "/etc/MailAliases";

char *
parse(ptr, buf)
	register char *ptr;
	char *buf;
{
	register char *cp = buf;

	while(isalnum(*ptr) || *ptr == '/' || *ptr == '-' ||
	      *ptr == '.' || *ptr == '*')
		*cp++ = *ptr++;
	*cp = 0;
	if(cp == buf)
		return 0;
	return buf;
}


aliascheck(name)
	char *name;
{
	register char *pp;
	char line[256], pbuf[64];
	register FILE *a;

	if((a = fopen(AliasFile, "r")) == NULL)
		return -1;
	while(fgets(line, sizeof line, a)) {
		if(line[0] == ';' || line[0] == '\n')   /* Comment Line */
			continue;
		if((pp = parse(line, pbuf)) == NULL) {
			fclose(a);
			return -1;
		}
		if(aleq(name, pp)) {
			fclose(a);
			return 1;
		}
	}
	fclose(a);
	return 0;
}


aleq(string, aliasent)
	register char *string, *aliasent;
{
	register int c;

	while(c = *string++)
		if(*aliasent == '*')
			return 1;
		else if((c|040) != (*aliasent|040))
			return(0);
		else
			aliasent++;
	return *aliasent == 0 || *aliasent == '*';
}
