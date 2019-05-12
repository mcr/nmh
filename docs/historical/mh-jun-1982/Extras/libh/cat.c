#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

char cat_buf[80];


char *cat(s1,s2)
char *s1, *s2;
{
	register char *r1, *r2, *bp;
	r1 = s1; r2 = s2;
	bp = cat_buf;
	while(*bp++ = *r1++);
	bp--;
	while(*bp++ = *r2++);
	return(cat_buf);
}
