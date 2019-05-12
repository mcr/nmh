#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/* return one line using getchar, caller must supply line buf and max. cnt */
/* return actual char cnt */

getline(line,cnt)
char line[];
int cnt;
{
	register char *lp;
	register c, cc;

	lp = line;
	cc = cnt;
	while ((c = getchar()) > 0) {
		*lp++ = c;
		if (--cc <= 0 || c == '\n') break;
	}
	return(cnt-cc);
}
