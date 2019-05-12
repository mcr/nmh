#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "mh.h";
#include "/rnd/borden/h/iobuf.h"
#include "/rnd/borden/h/stat.h"

/*#define DEBUG 1         /* Comment out normally */

/* Include a -msgid switch to .mh_profile to cause a
 * Message-Id component to be added to outgoing mail.
 */

char *anoyes[];         /* Std no/yes gans array        */
struct swit switches[] {
	"debug",         -1,      /* 0 */
	"format",         0,      /* 1 */
	"noformat",       0,      /* 2 */
	"msgid",          0,      /* 3 */
	"nomsgid",        0,      /* 4 */
	"verbose",        0,      /* 5 */
	"noverbose",      0,      /* 6 */
	"help",           4,      /* 7 */
	0,                0
};
struct iobuf in, pin, pout, fout, fccout;
int     format  1;              /* re-formatting is the default */

char    *logn, *parptr, *nmsg, fccfile[256];

main(argc, argv)
char *argv[];
{
	int debug;
	register char *cp, *msg;
	register int state;
	char *to, *cc, *bcc, *dist_to, *dist_cc, *dist_bcc;
	char *tolist, *cclist, *adrlist, *sender, *fcc, *dist_fcc;
	char name[NAMESZ], field[512];
	int specfld, dist, fcconly;
	int from, loc, net, verbose, msgid;
	int toseen, ccseen;
	char *ap;
	char *arguments[50], **argp;

	fout.b_fildes = dup(1);
	msgid = from = loc = net = to = cc = bcc = dist = fcc = dist_fcc =
		dist_to = dist_cc = dist_bcc = fcconly = debug =
		toseen = ccseen = 0;
	state = FLD;
	msg = 0;
	copyip(argv+1, arguments);
	argp = arguments;
	while(cp = *argp++) {
		if(*cp == '-')
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);       /* ambiguous */
				goto leave;
							/* unknown */
			case -1:printf("sndmsg: -%s unknown\n", cp);
				goto leave;
			case 0: verbose++; debug++; continue; /* -debug */
			case 1: format = 1; continue;         /* -format */
			case 2: format = 0; continue;         /* -noformat */
			case 3: msgid = 1;  continue;         /* -msgid */
			case 4: msgid = 0;  continue;         /* -nomsgid */
			case 5: loc = 1; net = 1; continue;   /* -verbose */
			case 6: loc = 0; net = 0; continue;   /* -noverbose */
			case 7: help("sndmsg [file]   [switches]",
				     switches);
				goto leave;
			}
		if(msg) {
			printf("Only one message at a time!\n");
			goto leave;
		} else
			msg = cp;
	}
	if(!msg) {
		printf("No Message specified.\n");
		goto leave;
	}
	if(fopen(msg, &in) < 0) {
		printf("Can't open \"%s\" for reading.\n", msg);
		goto leave;
	}

	state = FLD;

    for(;;)
	switch(state = m_getfld(state, name, field, sizeof field, &in)) {

	case FLD:
	case FLDEOF:
	case FLDPLUS:
		if(!dist && uleq(name, "to"))
			to = add(field, to);
		else if(!dist && uleq(name, "cc"))
			cc = add(field, cc);
		else if(!dist && uleq(name, "bcc"))
			bcc = add(field, bcc);
		else if(!dist && uleq(name, "fcc"))
			fcc = add(field, fcc);
		else if(uleq(name, "distribute-to"))
		{       dist++;         /* use presence of field as flag */
			dist_to = add(field, dist_to);
		}
		else if(uleq(name, "distribute-cc"))
			dist_cc = add(field, dist_cc);
		else if(uleq(name, "distribute-bcc"))
			dist_bcc = add(field, dist_bcc);
		else if(uleq(name, "distribute-fcc"))
			dist_fcc = add(field, dist_fcc);
		else if(uleq(name, "from"))
			from++;

		if(state == FLDEOF)
			goto done;
		continue;

	case BODY:
	case BODYEOF:
		goto done;

	default:
		printf("getfld returned %d\n", state);
		goto leave;
	}

done:
	if (dist)
	{       to = dist_to;
		cc = dist_cc;
		bcc = dist_bcc;
		fcc = dist_fcc;
	}
	if (!(to || cc))
		if(!fcc) {
			printf("Message %s has no addresses!!\n", msg);
			goto leave;
		} else
			fcconly++;
	pin.b_fildes = dup(2);
	pout.b_fildes = 3;

	if(to)
		if(parse(to, 'c'))
			goto leave;
	if(cc)
		if(parse(cc, 'c'))
			goto leave;
	if(bcc)
		if(parse(bcc, 'c'))
			goto leave;

	compress();
	adrlist = parptr; parptr = 0;
	if(verbose) {
		printf("Address List:\n%s", adrlist);
		flush();
	}

	if(to)
		if(parse(to, 'r', dist ? "Distribute-To: " : "To: "))
			goto leave;
	tolist = parptr; parptr = 0;
	if(cc)
		if(parse(cc, 'r', dist ? "Distribute-cc: " : "cc: "))
			goto leave;
	cclist = parptr; parptr = 0;

	if(verbose) {
		if (tolist) printf(tolist);
		if (cclist) printf(cclist);
		flush();
	}
	if(fcc) {
		if(*fcc == ' ')
			fcc++;
		for(cp = fcc; *cp && *cp != '\n'; cp++) ;
		*cp = 0;
		if(debug)
			printf("fcc: \"%s\"\n", fcc); flush();
		if((fccout.b_fildes = filemsg(fcc)) == -1)
			goto leave;
	}
	copy("\n", copy((logn = getlogn(getruid())), field));
	if(parse(field, 'r', dist ? "Distributed-By: " :
	       (from ? "Sender: ":"From: ")))
		goto leave;
	sender = parptr; parptr = 0;
	seek(in.b_fildes, 0, 0);
	if(debug)
		pout.b_fildes = 1;  /* Send msg to std output for debugging */
	else if(fcconly)
		pout.b_fildes = 0;  /* Flush send output if only an fcc */
	cputc('s', &pout);
	if(loc)
		cputc('l', &pout);
	if(net)
		cputc('n', &pout);
	cputc('\n', &pout);
	puts(logn, &pout);
	cputc('\n', &pout);
	puts(adrlist, &pout);
	puts("!\n", &pout);
	puts2(sender, &pout);
	puts2(dist ? "Distribution-Date: " : "Date: ", &pout);
	puts2(gd(), &pout);
	if(msgid) {
		puts2(dist ? "Distribution-ID: " : "Message-ID: ", &pout);
		puts2(gmid(), &pout);
	}
	seek(in.b_fildes, 0, 0);
	in.b_nleft = in.b_nextp = 0;

	state = FLD;

    for(;;)
	switch(state = m_getfld(state, name, field, sizeof field, &in)) {

	case FLD:
	case FLDEOF:
	case FLDPLUS:
		specfld = 0;
		if(format && uleq(name,dist ? "distribute-to":"to")) {
			if(!toseen) {
				toseen++;
				specfld++;
				if (tolist)
					puts2(tolist,&pout);
			}
		} else if (format && uleq(name,dist ?"distribute-cc":"cc")) {
			if(!ccseen) {
				ccseen++;
				specfld++;
				if (cclist)
					puts2(cclist,&pout);
			}
		} else if (uleq(name,dist ? "distribute-bcc" : "bcc") ||
		     uleq(name,dist ? "distributed-by" : "sender") ||
		     uleq(name,dist ? "distribution-date" : "date") ||
		     uleq(name,dist ? "distribution-id" : "message-id") ||
		     uleq(name,dist ? "distribution-fcc" : "fcc")) {
			specfld++;      /* Ignore these if present */
		} else {
			puts2(name, &pout);
			puts2(":", &pout);
			puts2(field, &pout);
		}
		while(state == FLDPLUS) {       /* read rest of field */
			state=m_getfld(state, name, field, sizeof field, &in);
			if (specfld) continue;
		   /*   puts2(name, &pout);
			puts2(":", &pout);  */
			puts2(field, &pout);
		}
		if(state == FLDEOF)
			goto endit;
		continue;
	case BODY:
	case BODYEOF:
		if(field[0]) {
			puts2("\n", &pout);
			puts2(field, &pout);
		}

		while(state == BODY) {
			state=m_getfld(state, name, field, sizeof field, &in);
			puts2(field, &pout);
		}
		if(state == BODYEOF)
			goto endit;
	default:
		printf("Error from getfld=%d\n", state);
		goto leave;
	}
endit:
	if(pout.b_fildes)
		fflush(&pout);
	if(fccout.b_fildes) {
		fflush(&fccout);
		close(fccout.b_fildes);
	}
	if(!debug && !fcconly) {
		write(3, "", 1);
		if((state = read(2, &field, sizeof field)) != 1 || field[0]) {
			printf("Error from adrparse.\n");
			goto leave;
		}
	}
	if(fccout.b_fildes)
		printf("Filed: %s:%s\n", fcc, nmsg);
	if(!debug) {
		if(!fcconly) {
			printf("Message %s sent.\n", msg);
			flush();
		}
		cp = copy(msg, field);
	    /*  for(cp = field; *cp++; ) ;  */
		cp[1] = 0;
		do
			*cp = cp[-1];
		while(--cp >= field && *cp != '/');
		*++cp = ',';                    /* New backup convention */
		unlink(field);
		if(link(msg, field) == -1 || unlink(msg) == -1)
			printf("Can't rename %s to %s\n", msg, field);
	}
	m_update();
	flush();
	done(0);
leave:
	printf("[Message NOT Delivered!]\n");
	if(fccout.b_fildes)
		unlink(fccfile);
	m_update();
	flush();
	done(1);
}


parse(ptr, type, fldname)
char *fldname;
{
	register int i;
	register int l;
	char line[128];

	putc(type, &pout);
	puts("\n", &pout);
	puts(ptr, &pout);
	fflush(&pout);
	write(3, "", 1);
	l = 0;

	while((i = getl(&pin, line, (sizeof line) - 1)) > 0) {
		line[i] = 0;
		if(line[0] == 0)
		{       if (type == 'r')
			{       if (l > 0)
					parptr = add("\n",parptr);
			}
			return(0);
		}
		else if(line[0] == '?') {
			printf("Adrparse: %s", line);
			return(1);
		}
		if (type == 'r')
		{       line[--i] = 0;
			if (l+i > 70)
			{       parptr = add("\n",parptr);
				l = 0;
			}
			if (l == 0)
			{       parptr = add(fldname,parptr);
				l =+ length(fldname);
			}
			else
			{       parptr = add(", ",parptr);
				l =+ 2;
			}
			parptr = add(line+1, parptr);
			l =+ i;
		}
		else
			parptr = add(line, parptr);
	}
	printf("Error from adrparse.\n");
	return(1);
}

gmid()
{
	static char bufmid[128];
	long now;
	register char *cp, *c2;
	register int *ip;

	cp = bufmid;
	*cp++ = '<';
	cp = copy(locv(0, getruid()), cp);
	*cp++ = '.';
	time(&now);
	cp = copy(locv(now), cp);
	cp = copy("@Rand-Unix>\n", cp);
	*cp = 0;
	return(bufmid);
}

#ifdef COMMENT
gmid()
{
	static char bufmid[128];
	long now;
	register char *cp, *c2;

	cp = bufmid;
	cp = copy("<[Rand-Unix]", cp);
	time(&now);
	c2 = cdate(&now);
	c2[9] = ' ';
	c2[18] = 0;
	cp = copy(c2, cp);
	*cp++ = '.';
	cp = copy(logn, cp);
	*cp++ = '>';
	*cp++ = '\n';
	*cp = 0;
	return(bufmid);
}
#endif

gd()
{
	register char *t, *p;
	register int *i;
	static bufgd[32];
	extern char *tzname[];
	long now;

	time(&now);
	t = ctime(&now);
	p = bufgd;

	*p++ = t[8];
	*p++ = t[9];
	*p++ = ' ';
	*p++ = t[4];
	*p++ = t[5];
	*p++ = t[6];
	*p++ = ' ';
	*p++ = t[20];
	*p++ = t[21];
	*p++ = t[22];
	*p++ = t[23];
	*p++ = ' ';
	*p++ = 'a';
	*p++ = 't';
	*p++ = ' ';
	*p++ = t[11];
	*p++ = t[12];
	*p++ = t[14];
	*p++ = t[15];
	*p++ = '-';
	i = localtime(&now);
	t = tzname[i[8]];
	*p++ = *t++;
	*p++ = *t++;
	*p++ = *t++;
	*p++ = '\n';
	*p++ = 0;
	return(bufgd);
}


getl(iob, buf, size)
{
	register char *cp;
	register int cnt, c;

	cp = buf; cnt = size;

	while((c = getc(iob)) >= 0) {
		*cp++ = c;
		--cnt;
		if(c == 0 || c == '\n' || cnt == 0)
			break;
	}
	return(size - cnt);
}


compress()
{
	register char *f1, *f2, *f3;

#ifdef DEBUG
	printf("compress:\n%s-----\n", parptr);
#endif
	for(f1 = parptr; *f1; ) {
		for(f2 = f1; *f2++ != '\n'; ) ;
		while(*f2) {
			if(eqqq(f1, f2)) {
				for(f3 = f2; *f3++ != '\n'; ) ;
				copy(f3, f2);
			} else
				while(*f2++ != '\n') ;
		}
		while(*f1++ != '\n') ;
	}
}


eqqq(s1, s2)
{
	register char *c1, *c2;

	c1 = s1; c2 = s2;
	while(*c1 != '\n' && *c2 != '\n')
		if((*c1++|040) != (*c2++|040))
			return(0);
	return((*c1|040) == (*c2|040));
}


filemsg(folder)
char *folder;
{
	register int i;
	register char *fp;
	struct inode stbuf;
	struct msgs *mp;

	fp = m_maildir(folder);
	if(stat(fp, &stbuf) < 0) {
		nmsg = concat("Create folder \"",
			fp, "\"? ", 0);
		if(!gans(nmsg, anoyes))
			return(-1);
		if(!makedir(fp)) {
			printf("Can't create folder.\n");
			return(-1);
		}
	}
	if(chdir(fp) < 0) {
		perror(concat("Can't chdir to ", fp, 0));
		return(-1);
	}
	if(!(mp = m_gmsg())) {
		printf("Can't read folder %s\n", folder);
		return(-1);
	}
	nmsg = m_name(mp->hghmsg + 1);
	copy(nmsg, copy("/", copy(m_maildir(fp), fccfile)));
	if((i = creat(fccfile, m_gmprot())) == -1)
		printf("Can't create %s\n", fccfile);
	return(i);
}


puts2(str, iob)
{
	puts(str, &fccout);
	puts(str, i