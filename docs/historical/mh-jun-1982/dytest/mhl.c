#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/*  THIS command is included wholesale by show.c, which calls
 *  its `main' rather than execing it when showproc is "mhl"
 */

#include <ctype.h>
#include <signal.h>
#include <setjmp.h>
#include <sgtty.h>
#ifndef  INCLUDED_BY_SHOW
#include <stdio.h>
#include "../mh.h"
#endif   INCLUDED_BY_SHOW
char *calloc();
extern char *index();
extern char *sprintf();

char    *ignores[25];
char    **igp = ignores;        /* List of ignored components           */
char    *parptr, *parse();
char    *fmtfile;               /* User specified format file           */
char    *folder;                /* Name of folder messages are in       */
char    *profargs[32];          /* Args extracted from profile          */
int     ofilec;                 /* Count of real output file args       */
int     ofilen;                 /* Number of output file                */
int     ontty;                  /* Output to char device                */
int     clearflg;               /* Overrides format screen clear        */
int     row, column;            /* For character output routine         */
int     alength, awidth;        /* -length and -width args              */
char    *oneline();
int     sigcatch();
int     exitstat;
jmp_buf env;

extern  char _sobuf[];          /* MLW  standard out buffer */
char    *strcpy();

/* Defines for c_flags (see below)                                      */
#define NOCOMPONENT     01      /* Don't show component name            */
#define UPPERCASE       02      /* Display in all upper case            */
#define CENTER          04      /* Center line within width             */
#define CLEARTEXT      010      /* Clear text line--simply copy to output */
#define PROCESSED      020      /* This item processed already          */
#define EXTRA          040      /* This message comp is an "extra"      */
#define HDROUTPUT     0100      /* This comp's hdr has been output      */
#define CLEARSCR      0200      /* Clear screen before each file        */
#define LEFTADJUST    0400      /* Left adjust mult lines of component  */
#define COMPRESS     01000      /* Compress text--ignore <lf's>         */

struct  comp  {
	struct comp  *c_next;   /* Chain to next                        */
	char         *c_name,   /* Component name                       */
		     *c_text,   /* Text associated with component       */
		     *c_ovtxt;  /* Line overflow indicator text         */
	int           c_offset, /* Left margin indent                   */
		      c_ovoff,  /* Line overflow indent                 */
		      c_width,  /* Width of field                       */
		      c_cwidth, /* Component width (default strlen(comp)) */
		      c_length; /* Length in lines                      */
	short         c_flags;  /* Special flags (see above)            */

} *msghd, *msgtl, *fmthd, *fmttl,
				/* Global contains global len/wid info  */
  global = { NULL, NULL, NULL,   "", 0, 0, 80,  0, 40, 0 },
  holder = { NULL, NULL, NULL, NULL, 0, 0,  0,  0,  0, NOCOMPONENT };

/* Defines for putcomp subrutine mode arg                               */
#define ONECOMP         0       /* Display only control comp name       */
#define BOTHCOMP        1       /* Display both comp names (conditionally) */

/*ARGSUSED*/
main(argc, argv)
	int argc;
	char *argv[];
{
	register struct comp *comp;
	register char *cp, **ap;
	FILE *fp;
	char line[256], name[64];
	int out();

	invo_name = argv[0];
	setbuf(stdout, _sobuf);
	VOID signal(SIGQUIT, out);
	ontty = gtty(1, (struct sgttyb *)name) != -1;
	cp = r1bindex(argv[0], '/');
	if((cp = m_find(cp)) != NULL) {
		ap = brkstring(getcpy(cp), " ", "\n");
		VOID copyip(ap, profargs);
/*              procargs(ap);*/
		procargs(profargs);
	}
	procargs(argv + 1);
	if(!folder)
		folder = getenv("mhfolder");
	if(fmtfile) {
		if((fp = fopen(m_maildir(fmtfile), "r")) == NULL) {
			fprintf(stderr, "mhl: Can't open format file: %s\n",
				fmtfile);
			done(1);
		}
	} else if((fp = fopen(m_maildir(mhlformat), "r")) == NULL &&
		  (fp = fopen(mhlstdfmt, "r")) == NULL) {
			fprintf(stderr, "mhl: Can't open default format file.\n");
			done(1);
	}
	while(fgets(line, sizeof line, fp)) {
		if(line[0] == ';')              /* Comment line */
			continue;

		line[strlen(line)-1] = 0;       /* Zap the <lf> */

		if(line[0] == ':') {            /* Clear text line */
			comp = (struct comp *) calloc(1, sizeof (struct comp));
			comp->c_text = getcpy(line+1);
			comp->c_flags = CLEARTEXT;
			comp->c_ovoff = -1;
			comp->c_cwidth = -1;
			goto fmtqueue;
		}

		parptr = line;
		strcpy(name, parse());
/***  printf("%s %c %s\n", name, *parptr, parptr+1);    */
		switch(*parptr) {

		case '\0':
		case ',':
		case '=':
			if(uleq(name, "ignores")) {
				igp = copyip(brkstring(getcpy(++parptr), ",", NULLCP), igp);
				continue;
			}
			parptr = line;
			while(*parptr) {
				if(evalvar(&global)) {
	fmterr:                         fprintf(stderr, "mhl: format file syntax error: %s\n",
						line);
					done(1);
				}
				if(*parptr)
					parptr++;
			}
			continue;

		case ':':
			comp = (struct comp *) calloc(1, sizeof (struct comp));
			comp->c_name = getcpy(name);
			comp->c_cwidth = -1;
			comp->c_ovoff = -1;
			while(*parptr == ':' || *parptr == ',') {
				parptr++;
				if(evalvar(comp))
					goto fmterr;
			}
	fmtqueue:       if(!fmthd)
				fmthd = fmttl = comp;
			else {
				fmttl->c_next = comp;
				fmttl = comp;
			}
			continue;

		default:
			goto fmterr;

		}
	}
	if(clearflg == 1)
		global.c_flags |= CLEARSCR;
	else if(clearflg == -1)
		global.c_flags &= ~CLEARSCR;
	if(awidth) global.c_width = awidth;
	if(alength) global.c_length = alength;
	if(global.c_width  < 5) global.c_width  = 10000;
	if(global.c_length < 5) global.c_length = 10000;
	VOID fclose(fp);
	if(!ofilec)
		process(NULLCP);
	else {
		for(ap = profargs; *ap; ap++)
			if(**ap)
				process(*ap);
		for(ap = argv+1; *ap; ap++)
			if(**ap)
				process(*ap);
	}
	done(exitstat);
}


evalvar(sp)
	register struct comp *sp;
{
	register char *cp;
	int c;
	char name[32];

	if(!*parptr)
		return 0;
	strcpy(name, parse());
/***printf("evalvar: %s %c %s\n", name, *parptr, parptr+1);     */
	if(uleq(name, "width")) {
		if(!*parptr++ == '=' || !*(cp = parse())) {
missing:                fprintf(stderr, "mhl: missing arg to variable %s\n",
				name);
			return 1;
		}
		sp->c_width = atoi(cp);
		return 0;
	}
	if(uleq(name, "compwidth")) {
		if(!*parptr++ == '=' || !*(cp = parse())) goto missing;
		sp->c_cwidth = atoi(cp);
		return 0;
	}
	if(uleq(name, "length")) {
		if(!*parptr++ == '=' || !*(cp = parse())) goto missing;
		sp->c_length = atoi(cp);
		return 0;
	}
	if(uleq(name, "overflowtext")) {
		if(!*parptr++ == '=') goto missing;
		cp = parptr;
		while(*parptr && *parptr != ':' && *parptr != ',') parptr++;
		c = *parptr;
		*parptr = 0;
		sp->c_ovtxt = getcpy(cp);
		*parptr = c;
		return 0;
	}
	if(uleq(name, "offset")) {
		if(!*parptr++ == '=' || !*(cp = parse())) goto missing;
		sp->c_offset = atoi(cp);
		return 0;
	}
	if(uleq(name, "overflowoffset")) {
		if(!*parptr++ == '=' || !*(cp = parse())) goto missing;
		sp->c_ovoff = atoi(cp);
		return 0;
	}
	if(uleq(name, "nocomponent")) {
		sp->c_flags |= NOCOMPONENT;
		return 0;
	}
	if(uleq(name, "uppercase")) {
		sp->c_flags |= UPPERCASE;
		return 0;
	}
	if(uleq(name, "center")) {
		sp->c_flags |= CENTER;
		return 0;
	}
	if(uleq(name, "clearscreen")) {
		sp->c_flags |= CLEARSCR;
		return 0;
	}
	if(uleq(name, "leftadjust")) {
		sp->c_flags |= LEFTADJUST;
		return 0;
	}
	if(uleq(name, "compress")) {
		sp->c_flags |= COMPRESS;
		return 0;
	}
	return 1;
}


char *
parse()
{
	static char result[64];
	register char *cp;
	register int c;

	cp = result;
	while(c = *parptr)
		if(isalnum(c) || c == '.' || c == '-' || c == '_') {
			*cp++ = c;
			parptr++;
		} else
			break;
	*cp = 0;
	return result;
}

struct swit switches[] = {
	"clear",                0,      /* 0 */
	"noclear",              0,      /* 1 */
	"folder folder",        0,      /* 2 */
	"form formfile",        0,      /* 3 */
	"length of page",       0,      /* 4 */
	"width of line",        0,      /* 5 */
	"help",                 4,      /* 6 */
	0,                      0
};

procargs(ap)
	register char **ap;
{
	register char *cp;

	while(cp = *ap++)
		if(*cp == '-') switch(smatch(++cp, switches)) {
		case -2:ambigsw(cp, switches);  /* ambiguous */
			done(1);
						/* unknown */
		case -1:fprintf(stderr, "mhl: -%s unknown\n", cp);
			done(1);
						/* -form format */
		case 0: clearflg = 1;           /* -clear       */
			ap[-1] = "";
			break;

		case 1: clearflg = -1;          /* -noclear     */
			ap[-1] = "";
			break;

		case 2: if(!(folder = *ap++) || *folder == '-') {
	missing: fprintf(stderr, "mhl: Missing arg for %s\n", ap[-2]);
				done(1);
			}
			ap[-2] = ""; ap[-1] = "";
			break;

		case 3: if(!(fmtfile = *ap++) || *fmtfile == '-')
				goto missing;
			ap[-2] = ""; ap[-1] = "";
			break;
						/* length */
		case 4: if(!(cp = *ap++) || *cp == '-')
				goto missing;
			alength = atoi(cp);
			ap[-2] = ""; ap[-1] = "";
			break;
						/* width */
		case 5: if(!(cp = *ap++) || *cp == '-')
				goto missing;
			awidth = atoi(cp);
			ap[-2] = ""; ap[-1] = "";
			break;
						/* -help        */
		case 6: help("mhl [switches] [files]", switches);
			done(0);

		} else
			ofilec++;
}

FILE *fp;

process(fname)
	char *fname;
{
	register int state;
	register struct comp *comp, *c2, *c3;
	char *cp, **ip, name[NAMESZ], buf[BUFSIZ];

	if(setjmp(env)) {
		discard(stdout);
		putchar('\n');
		goto out;
	}
	VOID signal(SIGINT, sigcatch);
	if(fname) {
		if((fp = fopen(fname, "r")) == NULL) {
			fprintf(stderr, "mhl: Can't open ");
			perror(fname);
			exitstat++;
			VOID signal(SIGINT, SIG_IGN);
			return;
		}
	} else
		fp = stdin;
	if(ontty) {
		strcpy(buf, "\n");
		if(ofilec > 1) {
			if(ofilen)
				printf("\n\n\n");
			printf("Press <return> to list \"");
			if(folder) printf("%s:", folder);
			printf("%s\"...", fname);
			VOID fflush(stdout);
			strcpy(buf, "");
			VOID read(1, buf, sizeof buf);
		}
		if(index(buf, '\n')) {
			if(global.c_flags & CLEARSCR)
				printf("\014\200");
		} else
			printf("\n");
	} else if(ofilec > 1) {
			if(ofilen)
				printf("\n\n\n");
			printf(">>> ");
			if(folder) printf("%s: ", folder);
			printf("%s\n\n", fname);
	}

	ofilen++;
	row = column = 0;
	msghd = 0;
	for(state = FLD ; ;) {
		state = m_getfld(state, name, buf, sizeof buf, fp);
		switch(state) {

		case FLD:
		case FLDEOF:
		case FLDPLUS:
			for(ip = ignores; *ip; ip++)
			    if(uleq(name, *ip)) {
				while(state == FLDPLUS)
				    state = m_getfld(state, name, buf, sizeof buf, fp);
				goto next;
			    }
			for(c3 = msghd; c3; c3 = c3->c_next)
			    if(uleq(name, c3->c_name))
				break;
			if(c3) {
			    comp = c3;
			    comp->c_text = add(buf, comp->c_text);
			} else {
			    comp = (struct comp *) calloc(1, sizeof (struct comp));
			    comp->c_name = getcpy(name);
			    comp->c_text = getcpy(buf);
			    comp->c_cwidth = -1;
			    comp->c_ovoff = -1;
			}
			while(state == FLDPLUS) {
			    state = m_getfld(state, name, buf, sizeof buf, fp);
			    comp->c_text = add(buf, comp->c_text);
			}
			for(c2 = fmthd; c2; c2 = c2->c_next)
				if(uleq(c2->c_name, comp->c_name))
					goto goodun;
			comp->c_flags |= EXTRA;
	   goodun:      if(!c3) {
			    if(!msghd)
				    msghd = msgtl = comp;
			    else {
				    msgtl->c_next = comp;
				    msgtl = comp;
			    }
			}
			if(state == FLDEOF)
			    goto doit;
			continue;

		default:
		case LENERR:
		case FMTERR:
			fprintf(stderr, "Message format error!\n");
			exitstat++;
			return;

		case BODY:
		case BODYEOF:
		case FILEEOF:
	doit:
			for(comp = fmthd; comp; comp = comp->c_next) {
			    if(comp->c_flags & CLEARTEXT) {
				putcomp(comp, comp, ONECOMP);
				continue;
			    }
			    if(uleq(comp->c_name, "messagename")) {
				cp = concat(fname, "\n", NULLCP);
				if(folder) {
				    holder.c_text = concat(folder, ":", cp, NULLCP);
				    free(cp);
				} else
				    holder.c_text = cp;
				putcomp(comp, &holder, ONECOMP);
				free(holder.c_text);
				holder.c_text = 0;
			    }
			    if(uleq(comp->c_name, "extras")) {
				for(c2 = msghd; c2; c2 = c2->c_next)
				    if(c2->c_flags & EXTRA)
					putcomp(comp, c2, BOTHCOMP);
				continue;
			    }
			    if(uleq(comp->c_name, "body")) {
				holder.c_text = buf;
				putcomp(comp, &holder, ONECOMP);
				holder.c_text = 0;
				while(state == BODY) {
				    state = m_getfld(state, name, buf, sizeof buf, fp);
				    holder.c_text = buf;
				    putcomp(comp, &holder, ONECOMP);
				    holder.c_text = 0;
				}
				continue;
			    }
			    for(c2 = msghd; c2; c2 = c2->c_next)
				if(uleq(c2->c_name, comp->c_name)) {
				    putcomp(comp, c2, ONECOMP);
				    break;
				}
			}
		out:
			if(fp)
				VOID fclose(fp);
			fp = NULL;
			if(holder.c_text) cndfree(holder.c_text);
			holder.c_text = 0;
			for(c2 = msghd; c2; c2 = comp) {
				comp = c2->c_next;
				cndfree(c2->c_name);
				cndfree(c2->c_text);
				free( (char *)c2);
			}
			msghd = msgtl = NULL;
			for(c2 = fmthd; c2; c2 = c2->c_next)
				c2->c_flags &= ~HDROUTPUT;
			VOID signal(SIGINT, SIG_IGN);
			return;
		}
next:   ;
	}
}

int     lm;             /* Left Margin for putstr               */
int     llim;           /* line limit for this component        */
int     wid;            /* width limit for this comp            */
int     ovoff;          /* overflow offset for this comp        */
char    *ovtxt;         /* overflow text for this comp          */
int     term;           /* term from last oneline()             */
char    *onelp;         /* oneline() text pointer               */

putcomp(cc, c2, flag)
	register struct comp *cc, *c2;
	int flag;
{
	register char *cp;
	int count, cchdr = 0;

#ifdef DEBUGCOMP
	    printf("%s(%o):%s:%s", cc->c_name, cc->c_flags, c2->c_name,
		c2->c_text);
#endif
	onelp = NULL;
	lm = 0;
	llim = cc->c_length? cc->c_length : -1;
	wid   = cc->c_width? cc->c_width : global.c_width;
	ovoff = cc->c_ovoff >= 0 ? cc->c_ovoff : global.c_ovoff;
	ovoff += cc->c_offset;
	ovtxt = cc->c_ovtxt ? cc->c_ovtxt : global.c_ovtxt;
	if(!ovtxt) ovtxt = "";
	if(wid < ovoff + strlen(ovtxt) + 5) {
		fprintf(stderr, "mhl: component: %s width too small for overflow.\n",
			cc->c_name);
		done(1);
	}
	if(cc->c_flags & CLEARTEXT) {
		putstr(cc->c_text);
		putstr("\n");
		return;
	}
	if(cc->c_flags & CENTER) {
		count = global.c_width;
		if(cc->c_width) count = cc->c_width;
		count -= cc->c_offset;
		count -= strlen(c2->c_text);
		if(!(cc->c_flags&HDROUTPUT) && !(cc->c_flags&NOCOMPONENT))
			count -= strlen(cc->c_name) + 2;
		lm = cc->c_offset+(count/2);
	} else if(cc->c_offset)
		lm = cc->c_offset;
	if(!(cc->c_flags & HDROUTPUT) && !(cc->c_flags & NOCOMPONENT)) {
		putstr(cc->c_name); putstr(": ");
		cc->c_flags |= HDROUTPUT;
		cchdr++;
		if((count = cc->c_cwidth - strlen(cc->c_name) - 2) > 0)
			while(count--) putstr(" ");
	}
	if(flag == BOTHCOMP && !(c2->c_flags & HDROUTPUT) &&
			       !(c2->c_flags & NOCOMPONENT)) {
		putstr(c2->c_name); putstr(": ");
		c2->c_flags |= HDROUTPUT;
	}
	if(cc->c_flags & UPPERCASE)
		for(cp = c2->c_text; *cp; cp++)
			if(islower(*cp))
				*cp -= 'a' - 'A';
	count = 0;
	if(cchdr)
		count = (cc->c_cwidth>=0) ? cc->c_cwidth : strlen(cc->c_name)+2;
	count += cc->c_offset;
	putstr(oneline(c2->c_text, cc->c_flags));
/***   if(cc->c_flags & COMPRESS) printf("-1-");        /***/
	if(term == '\n')
		putstr("\n");
	while(cp = oneline(c2->c_text, cc->c_flags)) {
/***   if(cc->c_flags & COMPRESS) printf("-2-");        /***/
		if(*cp) {
			lm = count;
			putstr(cp);
			if(term == '\n')
				putstr("\n");
		} else
			if(term == '\n')
				putstr("\n");
	}
	c2->c_flags |= PROCESSED;
}

putstr(string)
	register char *string;
{
	if(!column && lm > 0)
		while(lm > 0)
			if(lm >= 8) {
				putch('\t');
				lm -= 8;
			} else {
				putch(' ');
				lm--;
			}
	lm = 0;
	while(*string)
		putch(*string++);
}

putch(ch)
{
	char buf[32];

	if(llim == 0)
		return;
	switch(ch) {
	case '\n':
		if(llim > 0) llim--;
		column = 0;
		row++;
		if(ontty && row == global.c_length) {
			putchar('\007');
			VOID fflush(stdout);
			buf[0] = 0;
			VOID read(1, buf, sizeof buf);
			if(index(buf, '\n')) {
				if(global.c_flags & CLEARSCR) {
					putchar('\014');
					putchar('\200');
				}
				row = 0;
			} else {
				putchar('\n');
				row = global.c_length / 3;
			}
			return;
		}
		break;
	case '\t':
		column |= 07;
		column++;
		break;

	case '\010':
		column--;
		break;

	case '\r':
		column = 0;
		break;

	default:
		if(ch >= ' ')
			column++;
	}
	if(column >= wid) {
		putch('\n');
		if(ovoff > 0)
			lm = ovoff;
		if(ovtxt)
			putstr(ovtxt);
		else
			putstr("");
		putch(ch);
		return;
	}
	putchar(ch);
}


char *
oneline(stuff, flgs)
	char *stuff;
{
	register char *ret;
	register char *cp;
	int spc;

	if(!onelp)
		onelp = stuff;
	if(!*onelp) {
		onelp = 0;
		return NULL;
	}
	ret = onelp;
	term = 0;
	if(flgs & COMPRESS) {
		cp = ret;
		spc = 0;
		while(*onelp) {
			if(*onelp == '\n' || *onelp == '\t' || *onelp == ' '){
				if(*onelp == '\n' && !onelp[1]) {
					term = '\n';
					break;
				} else if(!spc) {
					*cp++ = ' ';
					spc++;
				}
			} else {
				*cp++ = *onelp;
				spc = 0;
			}
			onelp++;
		}
		*onelp = 0;
		*cp = 0;
	} else {
		while(*onelp && *onelp != '\n') onelp++;
		if(*onelp == '\n') {
			term = '\n';
			*onelp++ = 0;
		}
		if(flgs&LEFTADJUST)
			while(*ret == ' ' || *ret == '\t') ret++;
	}
	return ret;
}


sigcatch()
{
	longjmp(env, 1);
}


out()
{
	putchar('\n');
	VOID fflush(stdout);
	exit(-1);
}

discard(io)
register FILE *io;
{
	struct sgttyb sg;

	if (ioctl(fileno (io), TIOCGETP, &sg) >= 0)
		ioctl(fileno (io), TIOCSETP, &sg);
	io->_cnt = BUFSIZ;
	io->_ptr