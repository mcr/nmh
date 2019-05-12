#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>
#include <strings.h>
#include <signal.h>

/* #define NEWS 1 */
/* #define TEST 1 */

struct swit anyul[] = {
	"no",   0,
	"yes",  0,
	"use",  0,
	"list", 0,
	0
};

struct swit aleqs[] = {
	"list",                 0,      /* 0 */
	"edit [<editor>]",      0,      /* 1 */
	"quit [delete]",        0,      /* 2 */
	"send [switches]",      0,      /* 3 */
	0
};


struct swit switches[] = {
	"editor editor",        0,      /* 0 */
	"form formfile",        0,      /* 1 */
	"use",                  0,      /* 2 */
	"nouse",                0,      /* 3 */
	"help",                 4,      /* 4 */
	0,                      0
};

/*ARGSUSED*/
main(argc, argv)
char *argv[];
{
	register char *cp;
	register int in, out;
	int use, cnt, status;
	char buf[BUFSIZ], *ed, *file, *form;
	static char path[128];
	char **ap;
	char *arguments[50], **argp;

	invo_name = argv[0];
/***    setbuf(stdout, _sobuf); ***/
#ifdef NEWS
	m_news();
#endif
	form = 0; use = 0; file = 0; ed = 0;
	cp = r1bindex(argv[0], '/');
	if((cp = m_find(cp)) != NULL) {
		ap = brkstring(cp = getcpy(cp), " ", "\n");
		ap = copyip(ap, arguments);
	} else
		ap = arguments;
	VOID copyip(argv+1, ap);
	argp = arguments;
	while(cp = *argp++) {
		if(*cp == '-') {
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);  /* ambiguous */
				goto leave;
							/* unknown */
			case -1:fprintf(stderr, "comp: -%s unknown\n", cp);
				goto leave;
			case 0: if(!(ed = *argp++)) {   /* -editor */
      missing:  fprintf(stderr, "comp: Missing argument for %s switch\n", argp[-2]);
					goto leave;
				}
				continue;
			case 1: if(!(form = *argp++))   /* -form */
					goto missing;
				continue;
			case 2: use = 1;  continue;     /* -use */
			case 3: use = 0;  continue;     /* -nouse */
			case 4: help("comp    [file]  [switches]",
				     switches);
				goto leave;
			}
		}
		file = cp;
	}
	if(form) {
		if((in = open(m_maildir(form), 0)) < 0) {
			fprintf(stderr, "comp: Can't open form file: %s\n", form);
			goto leave;
		}
	} else if((in = open(m_maildir(components), 0)) < 0 &&
		   (in = open(stdcomps, 0)) < 0) {
			fprintf(stderr, "comp: Can't open default components file!!\n");
			goto leave;
	}
	if(!file)
		file = draft;
	VOID copy(m_maildir(file), path);
	if((out = open(path, 0)) >= 0) {
		cp = concat("\n\"", path, "\" exists; delete? ", 0);
		if(use || fdcompare(in, out))
			goto editit;
		while((status = gans(cp, anyul)) == 3)
				VOID showfile(path);
		if(status == 2) {
			use++;
			goto editit;
		}
		if(status == 0)
			goto leave;
		VOID close(out);
	} else if(use) {
		fprintf(stderr, "comp: \"%s\" doesn't exist!\n", path);
		goto leave;
	}
	if((out = creat(path, m_gmprot())) < 0) {
		fprintf(stderr, "comp: Can't create \"%s\"\n", path);
		goto leave;
	}
	do
		if(cnt = read(in, buf, sizeof buf))
			if(write(out, buf, cnt) != cnt) {
				fprintf(stderr, "comp: error writing ");
				perror(path);
				goto leave;
			}
	while(cnt == sizeof buf);
	VOID close(in);
editit:
	VOID close(out);
	if(m_edit(&ed, path, use, NULLCP) < 0)
		goto leave;
#ifdef TEST
	fprintf(stderr, "!! Test Version of SEND Being Run !!\n");
	fprintf(stderr, "   Send verbose !\n\n");
#endif

    for(;;) {
	if(!(argp = getans("\nWhat now? ", aleqs)))
		goto leave;
	switch(smatch(*argp, aleqs)) {
		case 0: VOID showfile(path);                    /* list */
			break;

		case 1: if(*++argp)                             /* edit */
				ed = *argp;
			if(m_edit(&ed, path, use, NULLCP) == -1)
				goto leave;
			break;
								/* quit */
		case 2: if(*++argp && (*argp[0] == 'd' ||
				       (*argp[0]=='-' && *argp[1]=='d')))
				if(unlink(path) == -1)  {
					fprintf(stderr, "Can't unlink %s ", path);
					perror("");
				}
			goto leave;

		case 3: VOID m_send(++argp, path);              /* send */
			goto leave;

		default:fprintf(stderr, "comp: illegal option\n");      /*##*/
			break;
	}
    }

leave:
	m_update();
	done(0);
}

