#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "mh.h"
#include "/rnd/borden/h/stat.h"
#include "/rnd/borden/h/iobuf.h"

char    mhnews[];
char    anoyes[];

m_news()
{
	struct inode stbf;
	struct iobuf in;
	register flag, c;
	long  readdate, gdate();
	char *cp, *ap;

	if(stat(mhnews, &stbf) == -1)
		return;
	m_getdefs();
	if((ap = m_find("newsdate")) != -1 ) {          /* seen news? */
		readdate = gdate(ap);
		if(stbf.i_mtime < readdate)             /* recently? */
			 return;
	}
	time(&readdate);                                /* current time */
	m_replace("newsdate", cdate(&readdate));        /* update profile */
	m_update();

	fopen(mhnews, &in);                             /* show news */
	flag = getc(&in);

	/**************************************************
	while((c = getc(&in)) != -1)
	 *      if(c == NEWSPAUSE) {
	 *              if(!gans("More? ", anoyes))
	 *                      break;
	 *      } else
	 *              putchar(c);
	 ***************************************************
	 */
	flush();
	showfile(mhnews);
	if(flag == NEWSHALT)
		exit(0);
	close(in.b_fildes);
}

