#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>
#include <errno.h>
#include <sgtty.h>
#include <signal.h>
#include <strings.h>

#define CKILL   006     /* @ => <CLOSE>         */
#define CERASE  001     /* # => <CTRL A>        */

int     wtuser;         /* waiting for user input */
int     sigint;         /* sensed an interrupt */
FILE    *in, *out;
struct  sgttyb sg;
struct swit switches[] = {
	"erase chr",      2,      /* 0 */ /* "2" can become "0",since no ed */
	"kill chr",       0,      /* 1 */
	"help",           4,      /* 2 */
	0,                0
};

extern  int errno;      /* MLW  4bsd does not have errno defined in errno.h */
extern  char _sobuf[];          /* MLW  standard out buffer */

/*ARGSUSED*/
main(argc, argv)
char *argv[];
{
	char tmpfil[32], *drft, name[NAMESZ], field[BUFSIZ];
	int exitstat;
	char skill, serase;
	char *killp, *erasep;
	register int i, state;
	register char *cp;
	char **ap;
	char *arguments[50], **argp;
	int sig();

	invo_name = argv[0];
	setbuf(stdout, _sobuf);
	tmpfil[0] = 0;
	skill = 0; exitstat = 0;
	cp = r1bindex(argv[0], '/');
	if((cp = m_find(cp)) != NULL) {
		ap = brkstring(cp = getcpy(cp), " ", "\n");
		ap = copyip(ap, arguments);
	} else
		ap = arguments;
	VOID copyip(argv+1, ap);
	argp = arguments;
	drft = NULLCP;
	while(cp = *argp++)
		if(*cp == '-')
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);       /* ambiguous */
				goto badleave;
							     /* unknown */
			case -1:fprintf(stderr, "prompter: -%s unknown\n", cp);
				goto badleave;
			case 0: if(!(erasep = *argp++)) {   /* -erase */
     missing:  fprintf(stderr, "prompter: Missing argument for %s switch\n", argp[-2]);
					goto badleave;
				}
				continue;
			case 1: if(!(killp= *argp++))       /* -kill */
					goto missing;
				continue;
							    /* -help */
			case 2: help("prompter    [switches]",
				     switches);
				goto badleave;
			}
		else if (!drft)
			drft = cp;
	if(!drft) {
		fprintf(stderr, "prompter: missing skeleton\n");
		goto badleave;
	}
	if((in = fopen(drft, "r")) == NULL) {
		fprintf(stderr, "Can't open %s\n", drft);
		goto badleave;
	}
	VOID copy(makename("prmt", ".tmp"), copy("/tmp/", tmpfil));
	if((out = fopen(tmpfil, "w")) == NULL) {
		fprintf(stderr, "Can't create %s\n", tmpfil);
		goto badleave;
	}
	VOID chmod(tmpfil, 0700);
	VOID signal(SIGINT, sig);
	VOID gtty(0, &sg);
	skill = sg.sg_kill;
	serase = sg.sg_erase;
	sg.sg_kill =    killp ?  chrcnv(killp) : skill;
	sg.sg_erase =   erasep ? chrcnv(erasep) : serase;
/***    stty(0, &sg);           ***/
	ioctl(0, TIOCSETN, &sg);
	if(killp || erasep) {
		printf("Erase Char="); chrdisp(sg.sg_erase);
		printf("; Kill Line="); chrdisp(sg.sg_kill);
		printf(".\n"); VOID fflush(stdout);
	}
	state = FLD;
	for(;;) switch(state = m_getfld(state,name,field,sizeof field,in)) {

	case FLD:
	case FLDEOF:
	case FLDPLUS:
		if(field[0] != '\n' || field[1] != 0) {
			printf("%s:%s", name, field);
			fprintf(out, "%s:%s", name, field);
			while(state == FLDPLUS) {
				state=m_getfld(state,name,field,sizeof field,in);
				printf("%s", field);
				fprintf(out, "%s", field);
			}
		} else {
			printf("%s: ", name);
			VOID fflush(stdout);
			i = getln(field);
			if(i == -1)
				goto badleave;
			if(i == 0 && (field[0] == '\n' || !field[0]))
				continue;
			fprintf(out, "%s:", name);
			do {
				if(field[0] != ' ' && field[0] != '\t')
					putc(' ', out);
				fputs(field, out);
			} while(i == 1 && (i = getln(field)) >= 0);
			if(i == -1)
				goto badleave;
		}
		field[0] = 0;
		if(state == FLDEOF)
			goto body;
		continue;

	case BODY:
	case BODYEOF:
	case FILEEOF:
  body:         fputs("--------\n", out);
		printf("--------\n");
		if(field[0]) {
			do {
				fputs(field, out);
				if(!sigint)
					printf("%s", field);
			} while(state == BODY &&
				(state=m_getfld(state,name,field,sizeof field,in)));
			printf("\n--------Enter additional text\n\n");
		}
		VOID fflush(stdout);
		for(;;) {
			VOID getln(field);
			if(field[0] == 0)
				break;
			fputs(field, out);
		}
		goto finish;

	default:
		fprintf(stderr, "Bad format file!\n");
		goto badleave;
	}


finish:
	printf("--------\n"); VOID fflush(stdout);
	VOID fclose(out);
	out = fopen(tmpfil, "r");
	VOID fclose(in);
	in = fopen(drft, "w");          /* Truncate prior to copy back */
	do
		if((i = read(fileno(out), field, sizeof field)) > 0)
			if(write(fileno(in), field, i) != i) {
				fprintf(stderr, "Write error to ");
				perror(drft);
				done(1);
			}
	while(i == sizeof field);
	goto leave;

badleave:
	exitstat = 1;

leave:
	if(in)
		VOID fclose(in);
	if(out)
		VOID fclose(out);
	if(tmpfil[0])
		VOID unlink(tmpfil);
	m_update();
	if(killp || erasep) {
		sg.sg_kill = skill;
		sg.sg_erase = serase;
/***            stty(0, &sg);           ***/
		ioctl(0, TIOCSETN, &sg);
	}
	done(exitstat);
}


getln(buf)
	char *buf;
{
	register char *cp;
	register int c;
	int stat;

	cp = buf;
	*cp = 0;
	wtuser = 1;
	for(;;) {
		c = getchar();
/***            fprintf(stderr,"getchar()=\\%o,errno=%d,EINTR=%d\n",c,errno,EINTR);/***/
		if(c == EOF)
			if(errno == EINTR) {
				stat = -1;
				goto leave;
			} else {
				stat = 0;
				goto leave;
			}
		if(c == '\n') {
			if(cp[-1] == '\\') {
				cp[-1] = c;
				stat = 1;
				goto leave;
			}
			*cp++ = c;
			*cp   = 0;
			stat = 0;
			goto leave;
		}
		if(cp < buf + 500)
			*cp++ = c;
		*cp = 0;
	}
 leave: wtuser = 0;
	return(stat);
	}


sig()
{
	VOID signal(SIGINT, sig);
	if(!wtuser)
		sigint = 1;
	return;
}


chrcnv(str)
char *str;
{
	register char *cp;
	register int c;

	cp = str;
	if((c = *cp++) != '\\')
		return(c);
	c = 0;
	while(*cp && *cp != '\n') {
		c *= 8;
		c += *cp++ - '0';
	}
	return c;
}


chrdisp(chr)
{
	register int c;

	c = chr;
	if(c < ' ')
		printf("<CTRL-%c>", c + '@');
	else
		printf("%c"