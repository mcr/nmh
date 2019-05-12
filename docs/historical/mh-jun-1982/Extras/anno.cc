#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "mh.h"

/* anno  [+folder] [msgs] [switches] -component component [-text [text]]
 *
 * prepends   Component: <<date stamp>>
 *                       text
 */

int fout;
struct msgs *mp;
int inplace;            /* preserve links in anno */

struct swit switches[] {
	"all",               -3,      /* 0 */
	"component",          0,      /* 1 */
	"inplace",            0,      /* 2 */
	"noinplace",          0,      /* 3 */
	"text",               0,      /* 4 */
	"help",               4,      /* 5 */
	0,                    0
};

main(argc, argv)
char *argv[];
{
	register int i, msgnum;
	register char *cp, *ap;
	char *folder, *maildir, *msgs[128], *component, *text;
	char *arguments[50], **argp;
	int msgp;

	fout = dup(1);
#ifdef NEWS
	m_news();
#endif
	folder = msgp = component = text = 0;
	ap = cp = argv[0];
	while(*cp)
		if(*cp++ == '/')
			ap = cp;
	if((cp = m_find(ap)) != -1) {
		ap = brkstring(cp = getcpy(cp), " ", "\n");
		ap = copyip(ap, arguments);
	} else
		ap = arguments;
	copyip(argv+1, ap);
	argp = arguments;
	while(cp = *argp++) {
		if(*cp == '-')
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);       /* ambiguous */
				goto leave;
							     /* unknown */
			case -1:printf("anno: -%s unknown\n", cp);
				goto leave;
							     /* -all */
			case 0: printf("\"-all\" changed to \"all\"\n");
				goto leave;
							     /* -component */
			case 1: if(component) {
					printf("Only one component.\n");
					goto leave;
				}
				if(!(component = *argp++)) {
		printf("anno: Missing argument for %s switch\n", argp[-2]);
					 goto leave;
				}
				continue;
			case 2: inplace = 1;  continue;      /* -inplace */
			case 3: inplace = 0;  continue;      /* -noinplace */
			case 4: if(text) {                   /* -text */
					printf("Only one text switch.\n");
					goto leave;
				}
				text = *argp++;  continue;
			case 5:                              /* -help */
help("anno   [+folder] [msgs] [switches] -component component [-text [text]]", switches);
				goto leave;
			}
		if(*cp == '+')
			if(folder) {
				printf("Only one folder at a time.\n");
				goto leave;
			} else
				folder = cp + 1;
		else
			msgs[msgp++] = cp;
	}
	if(!component) {
 usage:
  printf("usage: anno  [+folder] [msgs] [switches] -component component [-text [text]] \n");
		goto leave;
	}
	if(!msgp)
		msgs[msgp++] = "cur";
	if(!folder)
		folder = m_getfolder();
	maildir = m_maildir(folder);
	if(chdir(maildir) < 0) {
		printf("Can't chdir to: "); flush();
		perror(maildir);
		goto leave;
	}
	if(!(mp = m_gmsg(folder))) {
		printf("Can't read folder!?\n");
		goto leave;
	}
	if(mp->hghmsg == 0) {
		printf("No messages in \"%s\".\n", folder);
		goto leave;
	}
	for(msgnum = 0; msgnum < msgp; msgnum++)
		if(!m_convert(msgs[msgnum]))
			goto leave;
	if(mp->numsel == 0) {
		printf("anno: feta cheese and garlic\n"); /* never get here */
		goto leave;
	}
	m_replace("folder", folder);
	if(mp->lowsel != mp->curmsg)
		m_setcur(mp->lowsel);
	for(i = mp->lowsel; i <= mp->hghsel; i++)
		if(mp->msgstats[i] & SELECTED)
			annotate(m_name(i), component, text ? text : "");
 leave:
	m_update();
	flush();
}


/*
 *              if(*cp == '\"' )
 *                       if(astrp > 1)
 *                               goto usage;
 *                       else
 *                               if(!(ap = rmquote(cp + 1)))
 *                                       goto usage;
 *                               astrings[astrp++] = ap;
*rmquote(cp)
*char *cp;
*{
 *       int i;
 *
 *       if(cp[i = length(cp) -1] != '\"') {
 *               printf("rmquote: string doesn't end with \"\n");
 *               return(0);
 *       }
 *       cp[i] = 0;
 *       return(cp);
*}
		if(*cp == '=' )  {
			if(astrp > 1)
				goto usage;
			else
				astrings[astrp++] = cp + 1;
