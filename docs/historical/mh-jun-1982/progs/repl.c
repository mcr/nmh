#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include <whoami.h>
#include "../mh.h"
#include <stdio.h>
#include <signal.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../adrparse.h"

#define ERRHOST "?????"
/*#define NEWS 1*/

#define NOUSE 0

/* #define TEST 1 */

struct swit anyl[] = {
	"no",   0,
	"yes",  0,
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

short   anot;
#define OUTPUTLINELEN 72
short   outputlinelen = OUTPUTLINELEN;
short   ccme = 1;
struct msgs *mp;
char    *ed;
short   format = -1;            /* Default to re-format optionally*/
short   inplace;                /* preserve links in anno */
short   debug;
char    *badaddrs;

struct swit switches[] = {
	"annotate",           0,      /* 0 */
	"noannotate",         0,      /* 1 */
	"ccme",              -1,      /* 2 */
	"noccme",            -1,      /* 3 */
	"editor editor",      0,      /* 4 */
	"format",             0,      /* 5 */
	"noformat",           0,      /* 6 */
	"inplace",            0,      /* 7 */
	"noinplace",          0,      /* 8 */
	"width",              0,      /* 9 */
	"help",               4,      /*10 */
	"debug",              -5,     /*11 */
	0,                    0
};

char *ltrim();
char *rtrim();
char *niceadd();
char *fix();
char *addr();


/*ARGSUSED*/
main(argc, argv)
char *argv[];
{
	char *folder, *msg, *maildir;
	register char *cp, **ap, **argp;
	char *arguments[50];

	invo_name = argv[0];
#ifdef NEWS
	m_news();
#endif
	msg = 0; anot = 0; folder = 0;

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
			case -1:fprintf(stderr, "repl: -%s unknown\n", cp);
				goto leave;
			case 0: anot = 1;  continue;         /* -annotate */
			case 1: anot = 0;  continue;         /* -noannotate */
			case 2: ccme = 1;  continue;         /* -ccme */
			case 3: ccme = 0;  continue;         /* -noccme */
			case 4: if(!(ed = *argp++)) {        /* -editor */
missing:        fprintf(stderr, "repl: Missing argument for %s switch\n", argp[-2]);
					goto leave;
				}
				continue;
			case 5: format = 1; continue;        /* -format */
			case 6: format = 0; continue;        /* -noformat */
			case 7: inplace = 1;  continue;      /* -inplace */
			case 8: inplace = 0;  continue;      /* -noinplace */
			case 9: if(!(cp = *argp++) || *cp == '-')
					goto missing;
				outputlinelen = atoi(cp); continue;
							     /* -help */
			case 10:help("repl   [+folder] [msg] [switches]",
				     switches);
				goto leave;
			case 11:debug++; continue;           /* -debug */

			}
		if(*cp == '+') {
			if(folder) {
				fprintf(stderr, "Only one folder at a time.\n");
				goto leave;
			} else
				folder = path(cp+1, TFOLDER);
		} else if(msg) {
			fprintf(stderr, "Only one message per reply.\n");
			goto leave;
		} else
			msg = cp;
	}
	if(!m_find("path")) free(path("./", TFOLDER));
	if(!msg)
		msg = "cur";
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
	if(!m_convert(msg))
		goto leave;
	if(mp->numsel == 0) {
		fprintf(stderr, "repl: pepperoni pizza\n");/* never get here */
		goto leave;
	}
	if(mp->numsel > 1) {
		fprintf(stderr, "Only one message at a time.\n");
		goto leave;
	}
	m_replace(pfolder, folder);
	if(mp->lowsel != mp->curmsg)
		m_setcur(mp->lowsel);
	repl(getcpy(m_name(mp->lowsel)));
 leave:
	m_update();
	done(0);
}

char   *mn_rep;

repl(msg)
	char *msg;
{
	register char *cp;
	register int i;
	FILE *in, *out;
	char name[NAMESZ], field[BUFSIZ];
	char *drft, *msgid, *replto, *from, *sub, *date, *sender, *to, *cc;
	int state;
	int pid, wpid;
	char **argp;
	struct mailname *mnp = 0;
	struct stat stbuf;

/***/if(debug) printf("repl(%s)\n", msg);
	if((in = fopen(msg, "r")) == NULL) {
		fprintf(stderr, "Can't open "); perror(msg);
		return;
	}
	drft = m_maildir(draft);
/***/if(debug) printf("drft=%s\n", drft);
	if(stat(drft, &stbuf) != -1) {
		cp = concat("\"", drft, "\" exists; delete? ", 0);
		while((i = gans(cp, anyl)) == 2)
			VOID showfile(drft);
		if(!i)
			return;
		free(cp);
	}
	if((out = fopen(drft, "w")) == NULL) {
		fprintf(stderr, "Can't create \"%s\".\n", drft);
		return;
	}
	VOID chmod(drft, m_gmprot());

	state = FLD;
	to = cc = sender = replto = msgid = from = sub = date = 0;

    for(;;) {

	switch(state = m_getfld(state, name, field, sizeof field, in)) {

	case FLD:
	case FLDEOF:
	case FLDPLUS:
		if(uleq(name, "from")) {
			if(state == FLD && from) from = add(",",from);
			from = add(field, from);
			}
		if(uleq(name, "cc")) {
			if(state == FLD && cc) cc = add(",",cc);
			cc = add(field, cc);
			}
		if(uleq(name, "subject"))
			sub = add(field, sub);
		if(uleq(name, "date"))
			date = add(field, date);
		if(uleq(name, "to")) {
			if(state == FLD && to) to = add(",",to);
			to = add(field, to);
			}
		if(uleq(name, "message-id"))
			msgid = add(field, msgid);
		if(uleq(name, "reply-to")) {
			if(state == FLD && replto) replto = add(",",replto);
			replto = add(field, replto);
			}
		if(uleq(name, "sender")) {
			if(state == FLD && sender) sender = add(",",sender);
			sender = add(field, sender);
			}
		if(state == FLDEOF)
			goto done;
		break;

	case BODY:
	case BODYEOF:
	case FILEEOF:
		goto done;

	default:
		fprintf(stderr, "getfld returned %d\n", state);
		return;
	}

    }

done:
	VOID fclose(in);

	if(!(sender || from)) {
		fprintf(stderr, "repl: No Sender or From!?\n");
		return;
	}

	/* Pick up replacement host from "sender", else "from" */
#ifdef ARPANET
	if((cp = getname(sender ? sender : from)) == 0)
		return;
	if((mnp = getm(cp, HOSTNAME)) == 0)
		mn_rep = ERRHOST;
	else {
		if((mnp -> m_at) && ((*mnp -> m_at) == '!')) {  /* UUCP address */
			cp = stdhost((long)HOSTNUM);
		}
		else
			cp = stdhost(mnp->m_hnum);
		if(!cp) {
			fprintf(stderr, "repl: Unknown host: %s\n", mnp->m_host);
			return;
		}
		mn_rep = getcpy(cp);
		mnfree(mnp);
	}
#else
	mn_rep = getcpy (HOSTNAME);
#endif
	while(getname("")) ;    /* In case multi-name from/sender */

      /***/if(debug) printf("before testformats\n");
	/* Set format flag to 0 or 1 based on actual addresses, */
	/*  provided it hasn't been explicitly set */

	if(format == -1 && *mn_rep == '?') /* Oops, error in sender/from.*/
		format = 1;        /* Format! Use replacement host ERRHOST */
	if(format == -1) testformat(sender);
	if(format == -1) testformat(from);
	if(format == -1) testformat(replto);
	if(format == -1) testformat(to);
	if(format == -1) testformat(cc);
	if(format == -1) format = 0;    /* All LOCAL--don't format! */

/***/if(debug) printf("after testformats\n");
	if(!(from || replto)) {
		fprintf(stderr, "No one to reply to!!!\n");
		return;
	}
	outfmt(out, replto ? replto : from, "To", 0);

	if(to && cc)            /* Combine to & cc data */
		to = add(" ,", to);
	if(cc)
		to = add(cc, to);
	outfmt(out, to, "Cc", 1);

	if(sub) {                               /* Subject: Re: */
		fprintf(out, "Subject: ");
		if(*sub == ' ') sub++;
		if((sub[0] != 'R' && sub[0] != 'r') ||
		   (sub[1] != 'E' && sub[1] != 'e') ||
		   sub[2] != ':')
			fprintf(out, "Re: ");
		fprintf(out, sub);
	}
	if(date) {                              /* In-reply-to: */
		date[strlen(date)-1] = '.';
		if(*date == ' ') date++;
		fprintf(out, "In-reply-to: Your message of %s\n", date);
		if(msgid) {
			if(*msgid == ' ') msgid++;
			fprintf(out, "             %s", msgid);
		}
	}
	fprintf(out, "----------\n");
	if(badaddrs)
		fprintf(out, "\nREPL: CANT CONSTRUCT:\n%s\n", badaddrs);
	if(fclose(out) == EOF) {
		fprintf(stderr, "reply: Write error on ");
		perror(drft);
		return;
	}
	if(!debug)
		if(m_edit(&ed, drft, NOUSE, msg) < 0)
			return;
#ifdef TEST
	fprintf(stderr, "!! Test Version of SEND Being Run !!\n");
	fprintf(stderr, "   Send verbose !\n\n");
#endif

    for(;;) {
	if(!(argp = getans("\nWhat now? ", aleqs))) {
		VOID unlink("@");
		return;
	}
	switch(smatch(*argp, aleqs)) {
		case 0: VOID showfile(drft);                    /* list */
			break;

		case 1: if(*++argp)                             /* edit */
				ed = *argp;
			if(m_edit(&ed, drft, NOUSE, msg) == -1)
				return;
			break;

		case 2: if(*++argp && (*argp[0] == 'd' ||       /* quit */
				       (*argp[0]=='-' && *argp[1]=='d')))
				if(unlink(drft) == -1)  {
					fprintf(stderr, "Can't unlink %s ", drft);
					perror("");
				}
			return;

		case 3:                                         /* send */
			if(!mp->msgflags&READONLY) {    /* annotate first */
			    if(anot > 0) {
				while((pid = fork()) == -1) sleep(5);
				if(pid) {
					while((wpid=wait((int *)NULL))!= -1
					      && wpid!= pid);
					if(stat(drft, &stbuf) == -1)
						annotate(msg, "Replied", "", inplace);
					return;
				}
			    }
			}
			VOID m_send(++argp, drft);
			return;

		default:fprintf(stderr, "repl: illegal option\n");       /*##*/
			break;
	}
    }
}


outfmt(out, str, fn, chk)
	FILE *out;
	char *str, *fn;
	int chk;
{
	register char *cp;
	register struct mailname *mnp = 0;
	short ccoutput = 0, linepos = 0, len;

/***/if(debug)printf("outfmt mn_rep=%s\n", mn_rep);
	while(cp = getname(str)) {
		if(mnp) mnfree(mnp);
/***/if(debug)printf("Parse '%s'\n",cp);

		/* If couldn't find foreign default host due to sender/from */
		/* parse error, hand adrparse HOSTNAME for the time. */

		if((mnp = getm(cp, *mn_rep == '?' ? HOSTNAME : mn_rep)) == 0){
			char buf[512];
			/* Can't parse.  Add to badlist. */
			VOID sprintf(buf, " %s: %s\n", fn, cp);
			badaddrs = add(buf,  badaddrs);
			continue;
		} else if(*mn_rep == '?') {
			/*
			 * Now must distinguish between explicit-local and
			 * hostless-foreign, which parse identically
			 * because of the borrowed HOSTNAME.
			 * Kludge: If same string gives parse error when
			 * default host is ERRHOST, then original string was
			 * hostless (foreign).  Otherwise original string had
			 * HOSTNAME explicitly and is local.
			 */
			struct mailname *tmpmnp;

			if(tmpmnp = getm(cp, ERRHOST))
				mnfree(tmpmnp);
			else
				mnp->m_host = getcpy(ERRHOST);

		}
/***/if(debug) printf ("Host %D\n",mnp->m_hnum);
		if(chk && !ccme && uleq(mnp->m_mbox, getenv("USER")) &&
		   mnp->m_hnum == HOSTNUM)
			continue;
		if(!ccoutput) {
			fprintf(out, "%s: ", fn);
			linepos += (ccoutput = strlen(fn) + 2);
		}
		if((format && *mn_rep != '?')|| ((mnp->m_at && *mnp->m_at != '!') &&
			      (mnp->m_hnum != HOSTNUM)) ) {

/***/if(debug) printf ("That's not %d, so format it (name %s)\n",
			HOSTNUM, HOSTNAME);
			cp = adrformat(mnp,HOSTNAME);
/***/if(debug) printf ("Formatting produces '%s'\n",cp);
		}
		else if (format && *mn_rep == '?') {
			cp = adrformat(mnp, mn_rep);
		}
		else
			cp = mnp->m_text;
		len = strlen(cp);
		if(linepos != ccoutput)
			if(len + linepos + 2 > outputlinelen) {
				fprintf(out, ",\n%*s", ccoutput, "");
				linepos = ccoutput;
			} else {
				fputs(", ", out);
				linepos += 2;
			}
		fputs(cp, out);
		linepos += len;
	}
	if(mnp) mnfree(mnp);
	if(linepos) putc('\n', out);
}


testformat(str)
	char *str;
{
	register struct mailname *mnp;
	register char *cp;

	if(str)
		while(cp = getname(str)) {
			if(mnp = getm(cp, mn_rep)) {
				if((mnp->m_hnum != HOSTNUM) &&
				   (mnp->m_at) && (*mnp->m_at != '!'))
					format = 1;
				mnfree(m