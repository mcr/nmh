#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>

extern char *malloc();
extern char *sprintf();

#define Block

struct  msgs *mp;

/*  Look through a folder.
/*  Alloc a 'struct msgs' structure and fill it in
/*  with such things as what the low, high, and current messages are.
/*  Return a pointer to this structure or a null pointer if trouble.
/**/
struct msgs *
m_gmsg(name)
char *name;
{
	FILE *ifp;      /***/
	register int i, j;
	register char *cp;
	int  curfil;

	struct {
		struct {
			short d_inum;
			char d_name[14];
		} ent;
		int terminator;
	} dir;

	struct {
		int xhghmsg,
		    xnummsg,
		    xlowmsg,
		    xcurmsg;
		char xselist,
		     xflags,
		     xfiller,
		     xothers;
		char xmsgs[MAXFOLDER + 1];
	} msgbuf;

	if((ifp = fopen(".", "r")) == 0)
		return(0);
	for(j = 0; j <= MAXFOLDER; j++)
		msgbuf.xmsgs[j] = 0;
	msgbuf.xcurmsg = 0;
	msgbuf.xnummsg = 0;
	msgbuf.xselist = 0;
	msgbuf.xothers = 0;
	msgbuf.xlowmsg = 5000;
	msgbuf.xhghmsg = 0;
	msgbuf.xflags  = (access(".",2) == -1)? READONLY:0;  /*RAND sys call*/
	curfil = 0;
	dir.terminator = 0;
	cp = dir.ent.d_name;
	for(;;) {
		if(fread(&dir, sizeof dir.ent, 1, ifp) != 1)
			break;
		if(dir.ent.d_inum)
			if(j = mu_atoi(cp)) {
				if(j > msgbuf.xhghmsg)
					msgbuf.xhghmsg = j;
				msgbuf.xnummsg++;
				if(j < msgbuf.xlowmsg)
					msgbuf.xlowmsg = j;
				msgbuf.xmsgs[j] = EXISTS;
			} else if(*cp != ',' && *cp != '.')
					if(strcmp(cp, current) == 0)
						curfil++;
					else if(strcmp(cp, listname) == 0)
						msgbuf.xselist++;
					else
						msgbuf.xothers++;
	}
	if(!msgbuf.xhghmsg)
		msgbuf.xlowmsg = 0;
	VOID fclose(ifp);
	if(msgbuf.xflags&READONLY) Block {
		char buf[132];
		VOID sprintf(buf, "cur-%s", name);
/***            copy(name, copy("cur-", buf));          ***/
		if((cp = m_find(buf)) != NULL)
			if(j = mu_atoi(cp))
				msgbuf.xcurmsg = j;
	} else if(curfil && (i = open(current, 0)) >= 0) {
		if((j = read(i, dir.ent.d_name, sizeof dir.ent.d_name)) >= 2){
			dir.ent.d_name[j-1] = 0;    /* Zap <lf> */
			if(j = mu_atoi(dir.ent.d_name))
				msgbuf.xcurmsg = j;
		}
		VOID close(i);
	}
	Block {
		register struct msgs *msgp;
		if( (char *) (msgp = (struct msgs *)
		     malloc((unsigned) (sizeof *mp + msgbuf.xhghmsg + 2)))
		   == (char *) 0)
			return(0);
		msgp->hghmsg   = msgbuf.xhghmsg;
		msgp->nummsg   = msgbuf.xnummsg;
		msgp->lowmsg   = msgbuf.xlowmsg;
		msgp->curmsg   = msgbuf.xcurmsg;
		msgp->selist   = msgbuf.xselist;
		msgp->msgflags = msgbuf.xflags;
		msgp->others   = msgbuf.xothers;
		msgp->foldpath = name;
		msgp->lowsel   = 5000;
		msgp->hghsel   = 0;
		msgp->numsel   = 0;
		for(j = 0; j <= msgbuf.xhghmsg; j++)
			msgp->msgstats[j] = msgbuf.xmsgs[j];
		return(msgp);
	}
}
