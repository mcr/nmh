#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "mh.h"
#include "/rnd/borden/h/iobuf.h"
#include "/rnd/borden/h/signals.h"
#define NOUSE 0

/* #define TEST 1 */

char    draft[],
	sysed[],

char    *anyl[] {
	"no",   0,
	"yes",  0,
	"list", 0,
	0
};
char *aleqs[] {
	"list",              0,         /* 0 */
	"edit [<editor>]",   0,         /* 1 */
	"quit [delete]",     0,         /* 2 */
	"send [verbose]",    0,         /* 3 */
	0
};

int  *vec[MAXARGS], fout, anot;
int ccme 1;
char *maildir;
struct msgs *mp;
char *ed;
int inplace;            /* preserve links in anno */

struct swit switches[] {
	"annotate",           0,      /* 0 */
	"noannotate",         0,      /* 1 */
	"ccme",              -1,      /* 2 */
	"noccme",            -1,      /* 3 */
	"editor editor",      0,      /* 4 */
	"inplace",            0,      /* 5 */
	"noinplace",          0,      /* 6 */
	"help",               4,      /* 7 */
	0,                    0
};

main(argc, argv)
char *argv[];
{
	char *folder, *nfolder, *msg;
	register char *cp, *ap;
	register int cur;
	char *arguments[50], **argp;

	fout = dup(1);
#ifdef NEWS
	m_news();
#endif
	msg = anot = folder = 0;

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
			case -1:printf("resp: -%s unknown\n", cp);
				goto leave;
			case 0: anot = 1;  continue;         /* -annotate */
			case 1: anot = 0;  continue;         /* -noannotate */
			case 2: ccme = 1;  continue;         /* -ccme */
			case 3: ccme = 0;  continue;         /* -noccme */
			case 4: if(!(ed = *argp++)) {        /* -editor */
		printf("resp: Missing argument for %s switch\n", argp[-2]);
					goto leave;
				}
				continue;
			case 5: inplace = 1;  continue;      /* -inplace */
			case 6: inplace = 0;  continue;      /* -noinplace */
							     /* -help */
			case 7: help("resp   [+folder] [msg] [switches]",
				     switches);
				goto leave;
			}
		if(*cp == '+') {
			if(folder) {
				printf("Only one folder at a time.\n");
				goto leave;
			} else
				folder = cp + 1;
		} else if(msg) {
			printf("Only one message per response.\n");
			goto leave;
		} else
			msg = cp;
	}
	if(!msg)
		msg = "cur";
	if(!folder)
		folder = m_getfolder();
	maildir = getcpy(m_maildir(folder));
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
	if(!m_convert(msg))
		goto leave;
	if(mp->numsel == 0) {
		printf("resp: shark's fin soup\n");      /* never get here */
		goto leave;
	}
	if(mp->numsel > 1) {
		 printf("Only one message at a time.\n");
		 goto leave;
	}
	m_replace(pfolder, folder);
	if(mp->lowsel != mp->curmsg)
		m_setcur(mp->lowsel);
	repl(getcpy(m_name(mp->lowsel)));
 leave:
	m_update();
	flush();
}


repl(msg)
{
	register char *cp;
	register int i,j;
	struct iobuf in;
	char name[NAMESZ], field[512], *replto, *from, *cc, *sub, *date, *to;
	char *drft, *msgid;
	int state, out, status, intr;
	int pid, wpid;
	char **argp, *address;

	if(fopen(msg, &in) < 0) {
		printf("Can't open \"%s\"\n", msg);
		return;
	}
	drft = m_maildir(draft);
	if((out = open(drft, 0)) >= 0) {
		cp = concat("\"", drft, "\" exists; delete? ", 0);
		while((i = gans(cp, anyl)) == 2)
			showfile(drft);
		if(!i)
			return;
		free(cp);
		close(out);
	}
	if((out = creat(drft, m_gmprot())) < 0) {
		printf("Can't create \"%s\".\n", drft);
		return;
	}

	state = FLD;
	replto = msgid = to = from = cc = sub = date = 0;

    for(;;) {

	switch(state = m_getfld(state, name, field, sizeof field, &in)) {

	case FLD:
	case FLDEOF:
	case FLDPLUS:
		if(uleq(name, "from"))
			from = niceadd(field, from);
		if(uleq(name, "cc"))
			cc = niceadd(field, cc);
		if(uleq(name, "subject"))
			sub = niceadd(field, sub);
		if(uleq(name, "date"))
			date = niceadd(field, date);
		if(uleq(name, "to"))
			to = niceadd(field, to);
		if(uleq(name, "message-id"))
			msgid = niceadd(field, msgid);
		if(uleq(name, "reply-to"))
			replto = niceadd(field, replto);
	/*      if(uleq(name, "sender"))
			sender = niceadd(field, sender);        */
		if(state == FLDEOF)
			goto done;
		break;

	case BODY:
	case BODYEOF:
	case FILEEOF:
		goto done;

	default:
		printf("getfld returned %d\n", state);
		return;
	}

    }

done:

    /*  if(!(address = addr(sender)))
		if(!(address = addr(from)))
			address = addr(replto);
      */
	address = (replto ? addr(replto) : addr(from));
	if(!ccme)
		to = 0;
	if(!(from || replto || to)) {
		printf("No one to respond to!!!\n");
		return;
	}
	close(in.b_fildes);
	if(replto)
		rtrim(replto);
	if(from)
		rtrim(from);
	type(out, "To: ");                      /* To: */
	if(cp = (replto ? replto : from))
		type(out, cp);
	if(to) {
		if(cp)
			type(out, ",\n    ");
		if(address)
			to = fix(to, address);
		type(out, to);
	}
	if(cc) {                                /* cc */
		type(out, "cc: ");
		if(address)
			cc = fix(cc, address);
		type(out, cc);
	}
	if(sub) {                               /* Subject: Re: */
		type(out, "Subject: ");
		if(*sub == ' ') sub++;
		if((sub[0] != 'R' && sub[0] != 'r') ||
		   (sub[1] != 'E' && sub[1] != 'e') ||
		   sub[2] != ':')
			type(out, "Re: ");
		type(out, sub);
	}                                       /* In-response-to: */
	type(out, "In-response-to: Message from ");
	type(out, from);
	if(date) {
		date[length(date)-1] = '.';
		if(*date == ' ') date++;
		type(out, ", ");
		type(out, date);
	}
	type(out, "\n");
	if(msgid) {
		type(out, "                ");
		if(*msgid == ' ') msgid++;
		type(out, msgid);
	}
	type(out, "\t\t");
	type(out, maildir);
	type(out, "/");
	type(out, msg);
	type(out, "\n----------\n");
	close(out);

	if(m_edit(&ed, drft, NOUSE, msg) < 0)
		return;
#ifdef TEST
	printf("!! Test Version of SEND Being Run !!\n");
	printf("   Send verbose !\n\n");
#endif

    for(;;) {
	if(!(argp = getans("\nWhat now? ", aleqs))) {
		unlink("@");
		return;
	}
	switch(smatch(*argp, aleqs)) {
		case 0: showfile(drft);                         /* list */
			break;
		case 1: if(*++argp)                             /* edit */
				ed = *argp;
			if(m_edit(&ed, drft, NOUSE, msg) == -1)
				return;
			break;
		case 2: if(*++argp && *argp[0] == 'd')          /* quit */
				if(unlink(drft) == -1)  {
					printf("Can't unlink %s ", drft);
					perror("");
				}
			return;
		case 3: if(*++argp) cp = *argp;  else cp = "";  /* send */

			if(!mp->msgflags&READONLY) {    /* annotate first */
			    if(anot > 0) {
				while((pid = fork()) == -1) sleep(5);
				if(pid) {
					while(wpid=wait()!= -1 && wpid!= pid);
					if(stat(drft, field) == -1)
						annotate(msg, "Responded", "");
					return;
				}
			    }
			}
			if(!m_send(cp, drft))
				return;
		default:printf("resp: illegal option\n");       /*##*/
			break;
	}
