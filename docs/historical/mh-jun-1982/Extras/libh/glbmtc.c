#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#
/* glob match subroutine --

   "*" in params matches r.e ".*"
   "?" in params matches r.e. "."
   "[...]" in params matches character class
   "[...a-z...]" in params matches a through z.
*/

glbmtch(as, ap)
char *as, *ap;
{
	register char *s, *p;
	register scc;
	int c, cc, ok, lc;

	s = as;
	p = ap;
	if ((scc = *s++) && (scc =& 0177) == 0) scc = 0200;
	switch (c = *p++) {
	case '[':
		ok = 0;
		lc = 077777;
		while (cc = *p++) {
			if (cc==']') {
				if (ok) return(glbmtch(s, p));
				else return(0);
			} else if (cc=='-') {
				if (lc<=scc && scc<=(c = *p++)) ok++;
			} else if (scc == (lc=cc)) ok++;
		}
		return(0);
	default:
		if (c!=scc) return(0);
	case '?':
		if (scc) return(glbmtch(s, p));
		return(0);
	case '*':
		return(umatch(--s, p));
	case '\0':
		return(!scc);
	}
}

umatch(s, p)
char *s, *p;
{
	if(*p==0) return(1);
	while(*s) if (glbmtch(s++,p)) return(1);
	return(0);
}
