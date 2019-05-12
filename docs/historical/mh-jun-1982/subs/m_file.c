#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include "../folder.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

extern  int errno;

m_file(folder, msg, folders, nfolders, prsrvf, setcur)
char *folder;
char *msg;
struct st_fold folders[];
int nfolders;
int prsrvf;
int setcur;
{
	char newmsg[256], buf[BUFSIZ];
	register int i;
	register char *nmsg;
	register struct st_fold *fp;
	struct stat stbuf, stbf1;
	int n, o, linkerr;

    for(fp = folders; fp < &folders[nfolders]; fp++) {
	if(prsrvf)
		nmsg = msg;
	else {
		if (fp->f_mp->hghmsg >= MAXFOLDER) {
			fprintf(stderr,
				"Can't file msg %s -- folder %s is full.\n",
				msg, fp->f_name);
			return(1);
		}
		nmsg = m_name(fp->f_mp->hghmsg++ + 1);
	}
	VOID copy(nmsg, copy("/", copy(m_maildir(fp->f_name), newmsg)));
	if(link(msg, newmsg) < 0) {
		linkerr = errno;
		if(linkerr == EEXIST ||
		  (linkerr == EXDEV && stat(newmsg, &stbuf) != -1)) {
			if(linkerr != EEXIST || stat(msg, &stbf1) < 0 ||
			   stat(newmsg, &stbuf) < 0 ||
			   stbf1.st_ino != stbuf.st_ino) {
				fprintf(stderr, "Message %s:%s already exists.\n",
				     fp->f_name, msg);
				return(1);
			}
			continue;
		}
		if(linkerr == EXDEV) {
			if((o = open(msg, 0)) == -1) {
				fprintf(stderr, "Can't open %s:%s.\n",
					folder, msg);
				return(1);
			}
			VOID fstat(o, &stbuf);
			if((n = creat(newmsg, (int)stbuf.st_mode&0777)) == -1) {
				fprintf(stderr, "Can't create %s:%s.\n",
					fp->f_name, nmsg);
				VOID close(o);
				return(1);
			}
			do
				if((i=read(o, buf, sizeof buf)) < 0 ||
				  write(n, buf, i) == -1) {
				    fprintf(stderr, "Copy error on %s:%s to %s:%s!\n",
					    folder, msg, fp->f_name, nmsg);
				    VOID close(o); VOID close(n);
				    return(1);
				}
			while(i == sizeof buf);
			VOID close(n); VOID close(o);
		} else {
			fprintf(stderr, "Error on link %s:%s to %s:",
			    folder, msg, fp->f_name);
			perror(nmsg);
			return(1);
		}
	}
	if(   setcur
	   && (   (i = atoi(nmsg)) < fp->f_mp->curmsg
	       || !fp->f_mp->curmsg
	      )
	  )
		fp->f_mp->curmsg = i;
    }
    retur