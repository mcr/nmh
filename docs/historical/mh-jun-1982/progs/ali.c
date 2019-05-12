#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/types.h>

extern char *index();

#define EVERYONE 10

struct shome {          /* Internal name/uid/home database */
	struct shome *h_next;
	char *h_name;
	int   h_uid;
	int   h_gid;
	char *h_home;
} *homes, *home();

struct  mailname {
	struct mailname *m_next;
	char            *m_name;
}  addrlist;

char *malloc();

extern  char _sobuf[];          /* MLW  standard out buffer */

main(argc, argv)
	char **argv;
{
	register int i, list = 0;
	register struct mailname *lp;

	invo_name = argv[0];
	if(argc < 2) {
		printf("Usage: ali [-l] name ...\n");
		exit(1);
	}
	if(argc > 1 && strcmp(argv[1], "-l") == 0) {
		list++;
		argc--; argv++;
	}
	setbuf(stdout, _sobuf);
	gethomes();
	for(i = 1; i < argc; i++)
		insert(argv[i]);
	alias();                        /* Map names if needed */
	for(lp = addrlist.m_next; lp; lp = lp->m_next) {
		if(home(lp->m_name)) {
			if(list)
				printf("%s\n", lp->m_name);
			else {
				printf("%s%s", lp->m_name, lp->m_next?", ":"");
				if(stdout->_cnt < BUFSIZ - 65) {
					printf("\n"); VOID fflush(stdout);
				}
			}
		}
	}
	if(!list && stdout->_cnt && stdout->_cnt < BUFSIZ) {
		printf("\n"); VOID fflush(stdout);
	}
	for(lp = addrlist.m_next; lp; lp = lp->m_next)
		if(home(lp->m_name) == NULL)
			fprintf(stderr, "Ali: Unknown User: %s.\n", lp->m_name);

}

insert(name)
	char *name;
{
	register struct mailname *mp;
	char *getcpy();

/***    printf("insert(%s)\n", name);   ***/

	for(mp = &addrlist; mp->m_next; mp = mp->m_next)
		if(uleq(name, mp->m_next->m_name))
			return;         /* Don't insert existing name! */
	mp->m_next = (struct mailname *) malloc(sizeof *mp->m_next);
	mp = mp->m_next;
	mp->m_next = 0;
	mp->m_name = getcpy(name);
}

gethomes()
{
	register struct passwd *pw;
	register struct shome *h, *ph;
	struct   passwd *getpwent();
	char     *strcpy();

	ph = (struct shome *) &homes;
	while((pw = getpwent()) != NULL) {
		h = (struct shome *) malloc(sizeof *h);
		h->h_next = NULL;
		h->h_name = malloc((unsigned)strlen(pw->pw_name)+1);
		strcpy(h->h_name, pw->pw_name);
		h->h_uid = pw->pw_uid;
		h->h_gid = pw->pw_gid;
		h->h_home = malloc((unsigned)strlen(pw->pw_dir)+1);
		strcpy(h->h_home, pw->pw_dir);
		ph->h_next = h;
		ph = h;
	}
}

struct shome *
home(name)
	register char *name;
{
	register struct shome *h;

	if (index(name, '@') || 
	    index(name, ' ') ||
	    index(name, '!')) 
		return (homes);	/* WARNING! Depends on return value */
				/* 		being indifferent!  */

	for(h = homes; h; h = h->h_next)
		if(uleq(name, h->h_name))
			return(h);
	return(NULL);
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
	return(*aliasent == 0 | *aliasent == '*');
}


/* alias implementation below...
 */



#define GROUP   "/etc/group"
char    *AliasFile =    "/etc/MailAliases";

char *termptr;

char *
parse(ptr, buf)
	register char *ptr;
	char *buf;
{
	register char *cp;

	cp = buf;
	while(isspace(*ptr) || *ptr == ',' || *ptr == ':')
		ptr++;
	while(isalnum(*ptr) || *ptr == '/' || *ptr == '-' ||
	    *ptr == '!' || *ptr == '@' || *ptr == ' ' ||
	    *ptr == '.' || *ptr == '*')
		*cp++ = *ptr++;
	if(cp == buf) {
		switch(*ptr) {
		case '<':
		case '=':
			*cp++ = *ptr++;
		}
	}
	*cp = 0;
	if(cp == buf)
		return 0;
	termptr = ptr;
	return buf;
}

char *
advance()
{
	return(termptr);
}

alias()
{
	register char *cp, *pp;
	register struct mailname *lp;
	char line[256], pbuf[64];
	FILE *a;

	if((a = fopen(AliasFile, "r")) == NULL) {
		fprintf(stderr, "Can't open alias file ");
		perror(AliasFile);
		done(1);
	}
	while(fgets(line, sizeof line, a)) {
		if(line[0] == ';' || line[0] == '\n')   /* Comment Line */
			continue;
		if((pp = parse(line, pbuf)) == NULL) {
	    oops:       fprintf(stderr, "Bad alias file %s\n", AliasFile);
			fprintf(stderr, "Line: %s", line);
			done(1);
		}
		for(lp = &addrlist; lp->m_next; lp = lp->m_next) {
			if(aleq(lp->m_next->m_name, pp)) {
				remove(lp);
				if(!(cp = advance()) ||
				   !(pp = parse(cp, pbuf)))
					goto oops;
				switch(*pp) {
				case '<':       /* From file */
					cp = advance();
					if((pp = parse(cp, pbuf)) == NULL)
						goto oops;
					addfile(pp);
					break;
				case '=':       /* UNIX group */
					cp = advance();
					if((pp = parse(cp, pbuf)) == NULL)
						goto oops;
					addgroup(pp);
					break;
				case '*':       /* ALL Users */
					addall();
					break;
				default:        /* Simple list */
					for(;;) {
						insert(pp);
						if(!(cp = advance()) ||
						   !(pp = parse(cp, pbuf)))
							break;
					}
				}
				break;
			}
		}
	}
}


addfile(file)
	char *file;
{
	register char *cp, *pp;
	char line[128], pbuf[64];
	FILE *f;

/***    printf("addfile(%s)\n", file);          ***/
	if((f = fopen(file, "r")) == NULL) {
		fprintf(stderr, "Can't open ");
		perror(file);
		done(1);
	}
	while(fgets(line, sizeof line, f)) {
		cp = line;
		while(pp = parse(cp, pbuf)) {
			insert(pp);
			cp = advance();
		}
	}
	VOID fclose(f);
}

addgroup(group)
	char *group;
{
	register char *cp, *pp;
	int found = 0;
	char line[128], pbuf[64], *rindex();
	FILE *f;

/***    printf("addgroup(%s)\n", group);        ***/
	if((f = fopen(GROUP, "r")) == NULL) {
		fprintf(stderr, "Can't open ");
		perror(GROUP);
		done(1);
	}
	while(fgets(line, sizeof line, f)) {
		pp = parse(line, pbuf);
		if(strcmp(pp, group) == 0) {
			cp = rindex(line, ':');
			while(pp = parse(cp, pbuf)) {
				insert(pp);
				cp = advance();
			}
			found++;
		}
	}
	if(!found) {
		fprintf(stderr, "Group: %s non-existent\n", group);
		done(1);
	}
	VOID fclose(f);
}

addall()
{
	register struct shome *h;

/***    printf("addall()\n");                   ***/
	for(h = homes; h; h = h->h_next)
		if(h->h_uid >= EVERYONE)
			insert(h->h_name);
}

remove(mp)              /* Remove NEXT from argument node! */
	register struct mailname *mp;
{
	register struct mailname *rp;

	rp = mp->m_next;
	mp->m_next = rp->m_next;
	cndfree((char *)rp->m_name);
	cndfree((char *