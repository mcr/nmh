#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>
#include <signal.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>

#define NOUSE 0

/* #define TEST 1 */

struct swit anyl[] = {
	"no",   0,
	"yes",  0,
	"list", 0,
	0,
};

struct swit aleqs[] = {
	"list",                 0,      /* 0 */
	"edit [<editor>]",      0,      /* 1 */
	"quit [delete]",        0,      /* 2 */
	"send [switches]",      0,      /* 3 */
	0
};

struct msgs *mp;
char drft[128];
int inplace;            /* preserve links in anno */

struct swit switches[] = {
	"annotate",           0,      /* 0 */
	"noannotate",         0,      /* 1 */
	"editor editor",      0,      /* 2 */
	"form formfile",      0,      /* 3 */
	"inplace",            0,      /* 4 */
	"noinplace",          0,      /* 5 */
	"help",               4,      /* 6 */
	0,                    0
};

/*ARGSUSED*/
main(argc, argv)
char *argv[];
{
	char *folder, *maildir, *msgs[100], *ed, *form;
	register int msgnum;
	register char *cp, **ap;
	int msgp, anot;
	int in, out;
	int pid, wpid;
	char *arguments[50], **argp;

	invo_name = argv[0];
#ifdef NEWS
	m_news();
#endif
	anot = 0; folder = 0; msgp = 0; ed = 0; form = 0;
	cp = r1bindex(argv[0], '/');
	if((cp = m_find(cp)) != NULL) {
		ap = brkstring(cp = getcpy(cp), " ", "\n");
		ap = copyip(ap, arguments);
	} else
		ap = arguments;
	VOID copyip(argv+1, ap);
	argp = arguments;
	while(cp = *argp++) {
		if(*cp == '-')
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);       /* ambiguous */
				goto leave;
							     /* unknown */
			case -1:fprintf(stderr, "dist: -%s unknown\n", cp);
				goto leave;
			case 0: anot = 1;  continue;         /* -annotate */
			case 1: anot = 0;  continue;         /* -noannotate */
			case 2: if(!(ed = *argp++)) {        /* -editor */
      missing:  fprintf(stderr, "dist: Missing argument for %s switch\n", argp[-2]);
					goto leave;
				}
				continue;
			case 3: if(!(form = *argp++))        /* -form */
					goto missing;
				continue;
			case 4: inplace = 1;  continue;      /* -inplace */
			case 5: inplace = 0;  continue;      /* -noinplace */
							     /* -help */
			case 6: help("dist   [+folder] [msg] [switches]",
				     switches);
				goto leave;
			}
		if(*cp == '+') {
			if(folder) {
				fprintf(stderr, "Only one folder at a time.\n");
				goto leave;
			} else
				folder = path(cp+1, TFOLDER);
		} else
			msgs[msgp++] = cp;
	}
	if(!m_find("path")) free(path("./", TFOLDER));
	if(!msgp)
		msgs[msgp++] = "cur";
	if(!folder)
		folder = m_getfolder();
	maildir = m_maildir(folder);
	if(chdir(maildir) < 0) {
		fprintf(stderr, "Can't chdir to: ");
		perror(maildir);
		goto leave;
	}
	if(!(mp = m_gmsg(folder))) {
		fprintf(stderr, "Can't read folder!?\n");
		goto leave;
	}
	if(mp->hghmsg == 0) {
		fprintf(stderr, "No messages in \"%s\".\n", folder);
		goto leave;
	}
	for(msgnum = 0; msgnum < msgp; msgnum++)
		if(!m_convert((cp = msgs[msgnum])))
			goto leave;
	if(mp->numsel == 0) {
		fprintf(stderr, "dist: Tuna Melt\n");     /* never get here */
		goto leave;
	}
	if(mp->numsel > 1) {
		fprintf(stderr, "Only one message at a time.\n");
		goto leave;
	}
	if(form) {
		if((in = open(m_maildir(form), 0)) < 0) {
			fprintf(stderr, "dist: Can't open form file: %s\n", form);
			goto leave;
		}
	} else if(/***(in = open(m_maildir(distcomps), 0)) < 0 &&      ***/
		   (in = open(stddcomps, 0)) < 0) {
			fprintf(stderr, "dist: Can't open default components file!!\n");
			goto leave;
	}
	VOID copy(m_maildir(draft), drft);
	if((out = open(drft, 0)) >= 0) {
		if(!fdcompare(in, out)) {
			cp = concat("\"", drft, "\" exists; Delete? ", 0);
			while((msgnum = gans(cp, anyl)) == 2)
				VOID showfile(drft);
			if(!msgnum)
				return;
		}
		VOID close(out);
	}
	if((out = creat(drft, m_gmprot())) < 0) {
		fprintf(stderr, "Can't create \"%s\"\n", drft);
		goto leave;
	}
	cpydata(in, out);
	VOID close(in);
	if((in = open(cp = m_name(mp->lowsel), 0)) < 0) {
		fprintf(stderr, "Can't open message \"%s\"\n", cp);
		VOID unlink(drft);
		goto leave;
	}
	cpydata(in, out);
	VOID close(in);
	VOID close(out);
	m_replace(pfolder, folder);
	if(mp->lowsel != mp->curmsg)
		m_setcur(mp->lowsel);
	if(m_edit(&ed, drft, NOUSE, NULLCP) < 0)
		goto leave;
#ifdef TEST
	fprintf(stderr, "!! Test Version of SEND Being Run !!\n");
	fprintf(stderr, "   Send verbose !\n\n");
#endif

    for(;;) {
	if(!(argp = getans("\nWhat now? ", aleqs)))
		goto leave;
	switch(smatch(*argp, aleqs)) {
		case 0: VOID showfile(drft);                    /* list */
			break;

		case 1: if(*++argp)                             /* edit */
				ed = *argp;
			if(m_edit(&ed, drft, NOUSE, NULLCP) == -1)
				goto leave;
			break;

		case 2: if(*++argp && (*argp[0] == 'd' ||       /* quit */
				       (*argp[0]=='-' && *argp[1]=='d')))
				if(unlink(drft) == -1)  {
					fprintf(stderr, "Can't unlink %s ", drft);
					perror("");
				}
			goto leave;
		case 3:                                         /* send */
			if(!mp->msgflags&READONLY) {    /* annotate first */
			    if(anot > 0) {
				while((pid = fork()) == -1) sleep(5);
				if(pid) {
					while((wpid=wait((int *)NULL))!= -1
					      && wpid != pid);
					doano();
					goto leave;
				}
			    }
			}
			VOID m_send(++argp, drft);
			goto leave;

		default:fprintf(stderr, "dist: illegal option\n");       /*##*/
			break;
	}
    }

 leave:
	m_update();
	done(0);
}


cpydata(in, out)
{
	char buf[BUFSIZ];
	register int i;

	do
		if((i = read(in, buf, sizeof buf)) > 0)
			if(write(out, buf, i) != i) {
				fprintf(stderr, "dist: error on ");
				perror("write");
				done(1);
			}
	while(i == sizeof buf);
}


doano()
{
	register FILE *in;
	char name[NAMESZ], field[256];
	register int state;
	register char *text;

	if(stat(drft, (struct stat *)field) != -1) {
		fprintf(stderr, "%s not sent-- no annotations made.\n", drft);
		return;
	}
	text = copy(drft, field);
	text[1] = 0;
	do
		*text = text[-1];
	while(--text >= field && *text != '/');
	*++text = ',';                  /* New backup convention */
	if((in = fopen(field, "r")) == NULL) {
		fprintf(stderr, "Can't open %s\n", field);
		return;
	}
	state = FLD;
	text = 0;
   for(;;) switch(state = m_getfld(state, name, field, sizeof field, in)) {

	case FLD:
	case FLDEOF:
	case FLDPLUS:
		if(uleq(name, "distribute-to") ||
		   uleq(name, "distribute-cc") ) {
			if(state == FLD) {
				text = add(name, text);
				text = add(":", text);
			}
			text = add(field, text);
		}
		if(state == FLDEOF)
			goto out;
		continue;
	case BODY:
	case BODYEOF:
		goto out;
	default:
		fprintf(stderr, "Getfld returned %d\n", state);
		return;
	}

out:
	VOID fclose(in);
	annotate(m_name(mp->lowsel), "Distributed", text, inplace);
}
