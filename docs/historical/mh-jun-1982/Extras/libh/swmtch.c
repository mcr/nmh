#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/* switch match, or any unambiguous abbreviation */
/* exact match always wins, even if shares same root */
/* returns subscript in zero-terminated tbl[] of strings */
/* returns -1 if no match, -2 if ambiguous */

swmtch(string,tbl)
char *tbl[],*string;
{
	register char *sp, *tcp, c;
	char **tp;
	int firstone;

	firstone = -1;

	for (tp=tbl; tcp = *tp; tp++) {
		for (sp = string; *sp == *tcp++; ) {
			if (*sp++ == 0) return(tp-tbl); /* exact match */
		}
		if (*sp != 0) {
			if (*sp != ' ') continue; /* no match */
			if (*--tcp == 0) return(tp-tbl); /* exact match */
		}
		if (firstone == -1) firstone = tp-tbl; /* possible match */
		else firstone = -2;	/* ambiguous */
	}

	return (firstone);
}
