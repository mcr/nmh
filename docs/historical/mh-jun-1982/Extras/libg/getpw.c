#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif


struct buf {

	int	fildes;
	int	nleft;
	char	*nextp;
	char	buff[512];

};

int pich;


getpw(uid, buf)
char *buf;
{
	auto pbuf[259];
	register n, c;
	register char *bp;

	if (pich == 0) pich = open("/etc/passwd",0);
	if (pich <  0) return(1);
	seek(pich,0,0);
	pbuf->fildes = pich;
	pbuf->nleft = 0;
	uid =& 0377;            /* not for harv unix */

	for(;;) {
		bp = buf;
		while((c=pwgetc(&pbuf)) != '\n') {
			if (c <= 0) return(1);
			*bp++ = c;
		}
		*bp++ = '\0';
		bp = buf;
		n = 3;
		while(--n)
			while((c = *bp++) != ':')
				if (c == '\n') return(1);
		while((c = *bp++) != ':') {
			if (c < '0' || c > '9') continue;
			n = n * 10 + c - '0';
		}
		if (n == uid) return(0);
	}
	return(1);
}

pwgetc(buf)
struct buf *buf;
{
	register struct buf *bp;

	bp = buf;
	if (--bp->nleft < 0) {
		bp->nleft = read(bp->fildes,bp->buff,512)-1;
		if (bp->nleft < 0) return(-1);
		bp->nextp = bp->buff;
	}
	return(*bp->nextp++);
}
