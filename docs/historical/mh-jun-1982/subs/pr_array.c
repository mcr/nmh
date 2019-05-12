#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

pr_array(cp,ap)
char *cp,  **ap;
{
	register  int  i;

	for(i=0;  *ap;  ap++,i++)
		printf("%s[%d]=> %s\n", cp,i,*ap);
}

