#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../adrparse.h"

main(argc, argv)
	int argc;
	char *argv[];
{
	register struct mailname *mp;
	register char *cp;

	if(!(mp = getm(argv[1],"RAND-UNIX"))) {
		printf("adrparse returned 0\n");
		exit(0);
	}
	printf("%s\n", mp->m_text);
	printf("mbox: %s\n", mp->m_mbox);
	if(!mp->m_nohost) {
		printf("at: ");
		cp = mp->m_at;
		if(*cp == '@') putchar('@');
		else while(*cp != ' ') putchar(*cp++);
		printf("\n");
	}
	printf("host: %s ", mp->m_host);
	if(mp->m_nohost)
		printf("[default] ");
	else {
		putchar('"');
		for(cp = mp->m_hs; cp <= mp->m_he; )
			putchar(*cp++);
		putchar('"');
	}
	printf("\n");
	printf("hnum: %d\n", mp->m_hnum);
	printf("Proper: %s\n", adrformat(mp,"RAND-UNIX"));
}
